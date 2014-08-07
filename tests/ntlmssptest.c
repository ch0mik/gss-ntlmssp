/*
   GSS-NTLM

   Copyright (C) 2013 Simo Sorce <simo@samba.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"

#include "../src/gssapi_ntlmssp.h"
#include "../src/gss_ntlmssp.h"

const char *hex_to_str_8(uint8_t *d)
{
    static char hex_to_str_8_str[17];
    snprintf(hex_to_str_8_str, 17,
             "%02x %02x %02x %02x %02x %02x %02x %02x",
             d[0], d[1], d[2],  d[3],  d[4],  d[5],  d[6],  d[7]);
    return hex_to_str_8_str;
}

const char *hex_to_str_16(uint8_t *d)
{
    static char hex_to_str_16_str[33];
    snprintf(hex_to_str_16_str, 33,
             "%02x%02x%02x%02x%02x%02x%02x%02x"
             "%02x%02x%02x%02x%02x%02x%02x%02x",
             d[0], d[1], d[2],  d[3],  d[4],  d[5],  d[6],  d[7],
             d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
    return hex_to_str_16_str;
}

const char *hex_to_dump(uint8_t *d, size_t s)
{
    static char hex_to_dump_str[1024];
    char format[] = " %02x";
    size_t t, i, j, p;

    if (s > 256) t = 256;
    else t = s;

    snprintf(hex_to_dump_str, 4, format, d[0]);
    for (i = 1, p = 3; i < t; i++) {
        snprintf(&hex_to_dump_str[p], 4, format, d[i]);
        p += 3;
        if (((i + 1) % 16) == 0) {
            hex_to_dump_str[p++] = ' ';
            hex_to_dump_str[p++] = ' ';
            for (j = i - 15; j < i; j++) {
                if (isalnum(d[j])) hex_to_dump_str[p++] = d[j];
                else hex_to_dump_str[p++] = '.';
            }
            hex_to_dump_str[p++] = '\n';
            hex_to_dump_str[p] = '\0';
        }
    }
    if (t < s) {
        snprintf(&hex_to_dump_str[p], 7, " [..]\n");
    } else if (hex_to_dump_str[p] != '\n') {
        hex_to_dump_str[p] = '\n';
        hex_to_dump_str[p + 1] = '\0';
    }
    return hex_to_dump_str;
}

/* Test Data as per para 4.2 of MS-NLMP */
const char *T_User = "User";
const char *T_UserDom = "Domain";
const char *T_Passwd = "Password";
const char *T_Server_Name = "Server";
const char *T_Workstation = "COMPUTER";
uint8_t T_RandomSessionKey[] = {
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
};
uint64_t T_time = 0;
uint8_t T_ClientChallenge[] = {
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
};
uint8_t T_ServerChallenge[] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
};

/* NTLMv1 Auth Test Data */
struct {
    struct ntlm_key ResponseKeyLM;
    struct ntlm_key ResponseKeyNT;
    struct ntlm_key SessionBaseKey;
    uint8_t LMv1Response[24];
    uint8_t NTLMv1Response[24];
    struct ntlm_key KeyExchangeKey;
    struct ntlm_key EncryptedSessionKey1;
    struct ntlm_key EncryptedSessionKey2;
    struct ntlm_key EncryptedSessionKey3;
    uint32_t ChallengeFlags;
    uint8_t ChallengeMessage[0x44];
    uint8_t AuthenticateMessage[0xAC];
} T_NTLMv1 = {
    {
      .data = {
        0xe5, 0x2c, 0xac, 0x67, 0x41, 0x9a, 0x9a, 0x22,
        0x4a, 0x3b, 0x10, 0x8f, 0x3f, 0xa6, 0xcb, 0x6d
      },
      .length = 16
    },
    {
      .data = {
        0xa4, 0xf4, 0x9c, 0x40, 0x65, 0x10, 0xbd, 0xca,
        0xb6, 0x82, 0x4e, 0xe7, 0xc3, 0x0f, 0xd8, 0x52
      },
      .length = 16
    },
    {
      .data = {
        0xd8, 0x72, 0x62, 0xb0, 0xcd, 0xe4, 0xb1, 0xcb,
        0x74, 0x99, 0xbe, 0xcc, 0xcd, 0xf1, 0x07, 0x84
      },
      .length = 16
    },
    {
        0x98, 0xde, 0xf7, 0xb8, 0x7f, 0x88, 0xaa, 0x5d,
        0xaf, 0xe2, 0xdf, 0x77, 0x96, 0x88, 0xa1, 0x72,
        0xde, 0xf1, 0x1c, 0x7d, 0x5c, 0xcd, 0xef, 0x13
    },
    {
        0x67, 0xc4, 0x30, 0x11, 0xf3, 0x02, 0x98, 0xa2,
        0xad, 0x35, 0xec, 0xe6, 0x4f, 0x16, 0x33, 0x1c,
        0x44, 0xbd, 0xbe, 0xd9, 0x27, 0x84, 0x1f, 0x94
    },
    {
      .data = {
        0xb0, 0x9e, 0x37, 0x9f, 0x7f, 0xbe, 0xcb, 0x1e,
        0xaf, 0x0a, 0xfd, 0xcb, 0x03, 0x83, 0xc8, 0xa0
      },
      .length = 16
    },
    {
      .data = {
        0x51, 0x88, 0x22, 0xb1, 0xb3, 0xf3, 0x50, 0xc8,
        0x95, 0x86, 0x82, 0xec, 0xbb, 0x3e, 0x3c, 0xb7
      },
      .length = 16
    },
    {
      .data = {
        0x74, 0x52, 0xca, 0x55, 0xc2, 0x25, 0xa1, 0xca,
        0x04, 0xb4, 0x8f, 0xae, 0x32, 0xcf, 0x56, 0xfc
      },
      .length = 16
    },
    {
      .data = {
        0x4c, 0xd7, 0xbb, 0x57, 0xd6, 0x97, 0xef, 0x9b,
        0x54, 0x9f, 0x02, 0xb8, 0xf9, 0xb3, 0x78, 0x64
      },
      .length = 16
    },
    (
      NTLMSSP_NEGOTIATE_56 | NTLMSSP_NEGOTIATE_KEY_EXCH |
      NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_VERSION |
      NTLMSSP_TARGET_TYPE_SERVER |
      NTLMSSP_NEGOTIATE_ALWAYS_SIGN | NTLMSSP_NEGOTIATE_NTLM |
      NTLMSSP_NEGOTIATE_SEAL | NTLMSSP_NEGOTIATE_SIGN |
      NTLMSSP_NEGOTIATE_OEM | NTLMSSP_NEGOTIATE_UNICODE
    ),
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00,
        0x38, 0x00, 0x00, 0x00, 0x33, 0x82, 0x02, 0xe2,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x06, 0x00, 0x70, 0x17, 0x00, 0x00, 0x00, 0x0f,
        0x53, 0x00, 0x65, 0x00, 0x72, 0x00, 0x76, 0x00,
        0x65, 0x00, 0x72, 0x00
    },
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00,
        0x6c, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00,
        0x84, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00,
        0x48, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
        0x54, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x5c, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x9c, 0x00, 0x00, 0x00, 0x35, 0x82, 0x80, 0xe2,
        0x05, 0x01, 0x28, 0x0a, 0x00, 0x00, 0x00, 0x0f,
        0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00,
        0x69, 0x00, 0x6e, 0x00, 0x55, 0x00, 0x73, 0x00,
        0x65, 0x00, 0x72, 0x00, 0x43, 0x00, 0x4f, 0x00,
        0x4d, 0x00, 0x50, 0x00, 0x55, 0x00, 0x54, 0x00,
        0x45, 0x00, 0x52, 0x00, 0x98, 0xde, 0xf7, 0xb8,
        0x7f, 0x88, 0xaa, 0x5d, 0xaf, 0xe2, 0xdf, 0x77,
        0x96, 0x88, 0xa1, 0x72, 0xde, 0xf1, 0x1c, 0x7d,
        0x5c, 0xcd, 0xef, 0x13, 0x67, 0xc4, 0x30, 0x11,
        0xf3, 0x02, 0x98, 0xa2, 0xad, 0x35, 0xec, 0xe6,
        0x4f, 0x16, 0x33, 0x1c, 0x44, 0xbd, 0xbe, 0xd9,
        0x27, 0x84, 0x1f, 0x94, 0x51, 0x88, 0x22, 0xb1,
        0xb3, 0xf3, 0x50, 0xc8, 0x95, 0x86, 0x82, 0xec,
        0xbb, 0x3e, 0x3c, 0xb7
    }
};

/* NTLMv2 Auth Test Data */
struct {
    uint32_t ChallengeFlags;
    uint8_t TargetInfo[36];
    struct ntlm_key ResponseKeyNT;
    struct ntlm_key SessionBaseKey;
    uint8_t LMv2Response[16];
    uint8_t NTLMv2Response[16];
    struct ntlm_key EncryptedSessionKey;
    uint8_t ChallengeMessage[0x68];
    uint8_t AuthenticateMessage[0xE8];
} T_NTLMv2 = {
    (
      NTLMSSP_NEGOTIATE_56 | NTLMSSP_NEGOTIATE_KEY_EXCH |
      NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_VERSION |
      NTLMSSP_NEGOTIATE_TARGET_INFO |
      NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY |
      NTLMSSP_TARGET_TYPE_SERVER |
      NTLMSSP_NEGOTIATE_ALWAYS_SIGN | NTLMSSP_NEGOTIATE_NTLM |
      NTLMSSP_NEGOTIATE_SEAL | NTLMSSP_NEGOTIATE_SIGN |
      NTLMSSP_NEGOTIATE_OEM | NTLMSSP_NEGOTIATE_UNICODE
    ),
    {
      /* MSV_AV_NB_DOMAIN_NAME, 12 "D.o.m.a.i.n." */
      0x02, 0x00, 0x0c, 0x00, 0x44, 0x00, 0x6f, 0x00,
      0x6d, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6e, 0x00,
      /* MSV_AV_NB_COMPUTER_NAME, 12 "S.e.r.v.e.r." */
      0x01, 0x00, 0x0c, 0x00, 0x53, 0x00, 0x65, 0x00,
      0x72, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00,
      /* MSV_AV_EOL, 0 */
      0x00, 0x00, 0x00, 0x00
    },
    {
      .data = {
        0x0c, 0x86, 0x8a, 0x40, 0x3b, 0xfd, 0x7a, 0x93,
        0xa3, 0x00, 0x1e, 0xf2, 0x2e, 0xf0, 0x2e, 0x3f
      },
      .length = 16
    },
    {
      .data = {
        0x8d, 0xe4, 0x0c, 0xca, 0xdb, 0xc1, 0x4a, 0x82,
        0xf1, 0x5c, 0xb0, 0xad, 0x0d, 0xe9, 0x5c, 0xa3
      },
      .length = 16
    },
    {
        0x86, 0xc3, 0x50, 0x97, 0xac, 0x9c, 0xec, 0x10,
        0x25, 0x54, 0x76, 0x4a, 0x57, 0xcc, 0xcc, 0x19
    },
    {
        0x68, 0xcd, 0x0a, 0xb8, 0x51, 0xe5, 0x1c, 0x96,
        0xaa, 0xbc, 0x92, 0x7b, 0xeb, 0xef, 0x6a, 0x1c
    },
    {
      .data = {
        0xc5, 0xda, 0xd2, 0x54, 0x4f, 0xc9, 0x79, 0x90,
        0x94, 0xce, 0x1c, 0xe9, 0x0b, 0xc9, 0xd0, 0x3e
      },
      .length = 16
    },
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00,
        0x38, 0x00, 0x00, 0x00, 0x33, 0x82, 0x8a, 0xe2,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x24, 0x00, 0x24, 0x00, 0x44, 0x00, 0x00, 0x00,
        0x06, 0x00, 0x70, 0x17, 0x00, 0x00, 0x00, 0x0f,
        0x53, 0x00, 0x65, 0x00, 0x72, 0x00, 0x76, 0x00,
        0x65, 0x00, 0x72, 0x00, 0x02, 0x00, 0x0c, 0x00,
        0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00,
        0x69, 0x00, 0x6e, 0x00, 0x01, 0x00, 0x0c, 0x00,
        0x53, 0x00, 0x65, 0x00, 0x72, 0x00, 0x76, 0x00,
        0x65, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00
    },
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00,
        0x6c, 0x00, 0x00, 0x00, 0x54, 0x00, 0x54, 0x00,
        0x84, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x00,
        0x48, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x00,
        0x54, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x5c, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0xd8, 0x00, 0x00, 0x00, 0x35, 0x82, 0x88, 0xe2,
        0x05, 0x01, 0x28, 0x0a, 0x00, 0x00, 0x00, 0x0f,
        0x44, 0x00, 0x6f, 0x00, 0x6d, 0x00, 0x61, 0x00,
        0x69, 0x00, 0x6e, 0x00, 0x55, 0x00, 0x73, 0x00,
        0x65, 0x00, 0x72, 0x00, 0x43, 0x00, 0x4f, 0x00,
        0x4d, 0x00, 0x50, 0x00, 0x55, 0x00, 0x54, 0x00,
        0x45, 0x00, 0x52, 0x00, 0x86, 0xc3, 0x50, 0x97,
        0xac, 0x9c, 0xec, 0x10, 0x25, 0x54, 0x76, 0x4a,
        0x57, 0xcc, 0xcc, 0x19, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0x68, 0xcd, 0x0a, 0xb8,
        0x51, 0xe5, 0x1c, 0x96, 0xaa, 0xbc, 0x92, 0x7b,
        0xeb, 0xef, 0x6a, 0x1c, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x0c, 0x00, 0x44, 0x00, 0x6f, 0x00,
        0x6d, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6e, 0x00,
        0x01, 0x00, 0x0c, 0x00, 0x53, 0x00, 0x65, 0x00,
        0x72, 0x00, 0x76, 0x00, 0x65, 0x00, 0x72, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xc5, 0xda, 0xd2, 0x54, 0x4f, 0xc9, 0x79, 0x90,
        0x94, 0xce, 0x1c, 0xe9, 0x0b, 0xc9, 0xd0, 0x3e
    }
};

/* NTLMv2 Auth with Channel Bindings Test Data */
struct {
    uint32_t ChallengeFlags;
    const char *User;
    const char *Password;
    const char *Domain;
    const char *Workstation;
    const char *Server;
    const char *DnsDomain;
    const char *DnsServer;
    const char *Forest;
    uint64_t ServerTime;
    uint8_t ServerChallenge[8];
    struct ntlm_key NTLMHash;
    uint8_t TargetInfo[0xb6];
    uint8_t ChallengeMessage[0xfe];
    uint8_t AuthenticateMessage[0x228];
    uint8_t MIC[16];
    uint8_t CBSum[16];
} T_NTLMv2_CBT = {
    (
      NTLMSSP_NEGOTIATE_56 |
      NTLMSSP_NEGOTIATE_128 |
      NTLMSSP_NEGOTIATE_VERSION |
      NTLMSSP_NEGOTIATE_TARGET_INFO |
      NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY |
      NTLMSSP_TARGET_TYPE_DOMAIN |
      NTLMSSP_NEGOTIATE_ALWAYS_SIGN |
      NTLMSSP_NEGOTIATE_NTLM |
      NTLMSSP_REQUEST_TARGET |
      NTLMSSP_NEGOTIATE_UNICODE
    ),
    "Administrator",
    "P@ssw0rd",
    "WS2008R2",
    "WIN7-2-PC",
    "DC-WS2008R2",
    "ws2008r2.local",
    "DC-ws2008r2.ws2008r2.local",
    "ws2008r2.local",
    0x01cdde0bc33fe77b,
    { 0xa2, 0xc5, 0xe8, 0xca, 0x30, 0x84, 0xaa, 0x72 },
    {
      .data = {
        0xe1, 0x9c, 0xcf, 0x75, 0xee, 0x54, 0xe0, 0x6b,
        0x06, 0xa5, 0x90, 0x7a, 0xf1, 0x3c, 0xef, 0x42
      },
      .length = 16
    },
    {
        0x02, 0x00, 0x10, 0x00, 0x57, 0x00, 0x53, 0x00,
        0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x38, 0x00,
        0x52, 0x00, 0x32, 0x00, 0x01, 0x00, 0x16, 0x00,
        0x44, 0x00, 0x43, 0x00, 0x2d, 0x00, 0x57, 0x00,
        0x53, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00,
        0x38, 0x00, 0x52, 0x00, 0x32, 0x00, 0x04, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x03, 0x00,
        0x34, 0x00, 0x44, 0x00, 0x43, 0x00, 0x2d, 0x00,
        0x77, 0x00, 0x73, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x72, 0x00, 0x32, 0x00,
        0x2e, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x05, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x07, 0x00,
        0x08, 0x00, 0x7b, 0xe7, 0x3f, 0xc3, 0x0b, 0xde,
        0xcd, 0x01, 0x00, 0x00, 0x00, 0x00
    },
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x38, 0x00, 0x00, 0x00, 0x05, 0x82, 0x89, 0xa2,
        0xa2, 0xc5, 0xe8, 0xca, 0x30, 0x84, 0xaa, 0x72,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xb6, 0x00, 0xb6, 0x00, 0x48, 0x00, 0x00, 0x00,
        0x06, 0x01, 0xb0, 0x1d, 0x00, 0x00, 0x00, 0x0f,
        0x57, 0x00, 0x53, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x52, 0x00, 0x32, 0x00,
        0x02, 0x00, 0x10, 0x00, 0x57, 0x00, 0x53, 0x00,
        0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x38, 0x00,
        0x52, 0x00, 0x32, 0x00, 0x01, 0x00, 0x16, 0x00,
        0x44, 0x00, 0x43, 0x00, 0x2d, 0x00, 0x57, 0x00,
        0x53, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00,
        0x38, 0x00, 0x52, 0x00, 0x32, 0x00, 0x04, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x03, 0x00,
        0x34, 0x00, 0x44, 0x00, 0x43, 0x00, 0x2d, 0x00,
        0x77, 0x00, 0x73, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x72, 0x00, 0x32, 0x00,
        0x2e, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x05, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x07, 0x00,
        0x08, 0x00, 0x7b, 0xe7, 0x3f, 0xc3, 0x0b, 0xde,
        0xcd, 0x01, 0x00, 0x00, 0x00, 0x00
    },
    {
        0x4e, 0x54, 0x4c, 0x4d, 0x53, 0x53, 0x50, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00,
        0x94, 0x00, 0x00, 0x00, 0x7c, 0x01, 0x7c, 0x01,
        0xac, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,
        0x58, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x1a, 0x00,
        0x68, 0x00, 0x00, 0x00, 0x12, 0x00, 0x12, 0x00,
        0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x28, 0x02, 0x00, 0x00, 0x05, 0x82, 0x88, 0xa2,
        0x06, 0x01, 0xb0, 0x1d, 0x00, 0x00, 0x00, 0x0f,
        0xf0, 0x54, 0xa5, 0x42, 0xb0, 0x90, 0xb6, 0x6c,
        0x1f, 0xea, 0x1a, 0x2c, 0xc8, 0x2e, 0x93, 0x0b,
        0x57, 0x00, 0x53, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x52, 0x00, 0x32, 0x00,
        0x41, 0x00, 0x64, 0x00, 0x6d, 0x00, 0x69, 0x00,
        0x6e, 0x00, 0x69, 0x00, 0x73, 0x00, 0x74, 0x00,
        0x72, 0x00, 0x61, 0x00, 0x74, 0x00, 0x6f, 0x00,
        0x72, 0x00, 0x57, 0x00, 0x49, 0x00, 0x4e, 0x00,
        0x37, 0x00, 0x2d, 0x00, 0x32, 0x00, 0x2d, 0x00,
        0x50, 0x00, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x5a, 0x6a, 0x21, 0xae,
        0x1a, 0x44, 0xc0, 0x44, 0x69, 0x3e, 0xee, 0x59,
        0xfc, 0x5d, 0x81, 0xe0, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7b, 0xe7, 0x3f, 0xc3,
        0x0b, 0xde, 0xcd, 0x01, 0x27, 0xfc, 0x11, 0x80,
        0x82, 0xc2, 0xfb, 0xdd, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x10, 0x00, 0x57, 0x00, 0x53, 0x00,
        0x32, 0x00, 0x30, 0x00, 0x30, 0x00, 0x38, 0x00,
        0x52, 0x00, 0x32, 0x00, 0x01, 0x00, 0x16, 0x00,
        0x44, 0x00, 0x43, 0x00, 0x2d, 0x00, 0x57, 0x00,
        0x53, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00,
        0x38, 0x00, 0x52, 0x00, 0x32, 0x00, 0x04, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x03, 0x00,
        0x34, 0x00, 0x44, 0x00, 0x43, 0x00, 0x2d, 0x00,
        0x77, 0x00, 0x73, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x72, 0x00, 0x32, 0x00,
        0x2e, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x05, 0x00,
        0x1c, 0x00, 0x77, 0x00, 0x73, 0x00, 0x32, 0x00,
        0x30, 0x00, 0x30, 0x00, 0x38, 0x00, 0x72, 0x00,
        0x32, 0x00, 0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00,
        0x63, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x07, 0x00,
        0x08, 0x00, 0x7b, 0xe7, 0x3f, 0xc3, 0x0b, 0xde,
        0xcd, 0x01, 0x06, 0x00, 0x04, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x08, 0x00, 0x30, 0x00, 0x30, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x4d, 0x6b,
        0x0d, 0x27, 0x54, 0x10, 0x22, 0xf5, 0xff, 0xa6,
        0x73, 0xda, 0x2b, 0xfc, 0xfd, 0xf1, 0x94, 0x2f,
        0x25, 0x7b, 0xe1, 0x1a, 0x49, 0xc9, 0x54, 0x19,
        0x7a, 0xca, 0x8a, 0xaf, 0x2e, 0xaf, 0x0a, 0x00,
        0x10, 0x00, 0x65, 0x86, 0xe9, 0x9d, 0x81, 0xc2,
        0xfc, 0x98, 0x4e, 0x47, 0x17, 0x2f, 0xd4, 0xdd,
        0x03, 0x10, 0x09, 0x00, 0x3e, 0x00, 0x48, 0x00,
        0x54, 0x00, 0x54, 0x00, 0x50, 0x00, 0x2f, 0x00,
        0x64, 0x00, 0x63, 0x00, 0x2d, 0x00, 0x77, 0x00,
        0x73, 0x00, 0x32, 0x00, 0x30, 0x00, 0x30, 0x00,
        0x38, 0x00, 0x72, 0x00, 0x32, 0x00, 0x2e, 0x00,
        0x77, 0x00, 0x73, 0x00, 0x32, 0x00, 0x30, 0x00,
        0x30, 0x00, 0x38, 0x00, 0x72, 0x00, 0x32, 0x00,
        0x2e, 0x00, 0x6c, 0x00, 0x6f, 0x00, 0x63, 0x00,
        0x61, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    },
    {
        0xf0, 0x54, 0xa5, 0x42, 0xb0, 0x90, 0xb6, 0x6c,
        0x1f, 0xea, 0x1a, 0x2c, 0xc8, 0x2e, 0x93, 0x0b
    },
    {
        0x65, 0x86, 0xE9, 0x9D, 0x81, 0xC2, 0xFC, 0x98,
        0x4E, 0x47, 0x17, 0x2F, 0xD4, 0xDD, 0x03, 0x10
    },
};

struct t_gsswrapex_data {
    uint32_t flags;
    struct ntlm_buffer Plaintext;
    struct ntlm_key KeyExchangeKey;
    struct ntlm_key ClientSealKey;
    struct ntlm_key ClientSignKey;
    struct ntlm_buffer Ciphertext;
    struct ntlm_buffer Signature;
};

/* Basic GSS_WrapEx V1 Test Data */

uint8_t T_GSSWRAPv1noESS_Plaintext_data[18] = {
    0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6e,
    0x00, 0x74, 0x00, 0x65, 0x00, 0x78, 0x00, 0x74, 0x00
};
uint8_t T_GSSWRAPv1noESS_Ciphertext_data[18] = {
    0x56, 0xfe, 0x04, 0xd8, 0x61, 0xf9, 0x31, 0x9a, 0xf0,
    0xd7, 0x23, 0x8a, 0x2e, 0x3b, 0x4d, 0x45, 0x7f, 0xb8
};
uint8_t T_GSSWRAPv1noESS_Signature_data[16] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x09, 0xdc, 0xd1, 0xdf, 0x2e, 0x45, 0x9d, 0x36
};

struct t_gsswrapex_data T_GSSWRAPv1noESS = {
    (
      NTLMSSP_NEGOTIATE_56 |
      NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL
    ),
    {
      .data = T_GSSWRAPv1noESS_Plaintext_data,
      .length = sizeof(T_GSSWRAPv1noESS_Plaintext_data)
    },
    {
      .data = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
      },
      .length = 16
    },
    {
      .data = { 0 },
      .length = 0
    },
    {
      .data = { 0 },
      .length = 0
    },
    {
      .data = T_GSSWRAPv1noESS_Ciphertext_data,
      .length = sizeof(T_GSSWRAPv1noESS_Ciphertext_data)
    },
    {
      .data = T_GSSWRAPv1noESS_Signature_data,
      .length = sizeof(T_GSSWRAPv1noESS_Signature_data)
    },
};

/* GSS_WrapEx V1 Extended Session Security Test Data */

uint8_t T_GSSWRAPEXv1_Plaintext_data[18] = {
    0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6e,
    0x00, 0x74, 0x00, 0x65, 0x00, 0x78, 0x00, 0x74, 0x00
};
uint8_t T_GSSWRAPEXv1_Ciphertext_data[18] = {
    0xa0, 0x23, 0x72, 0xf6, 0x53, 0x02, 0x73, 0xf3, 0xaa,
    0x1e, 0xb9, 0x01, 0x90, 0xce, 0x52, 0x00, 0xc9, 0x9d
};
uint8_t T_GSSWRAPEXv1_Signature_data[16] = {
    0x01, 0x00, 0x00, 0x00, 0xff, 0x2a, 0xeb, 0x52,
    0xf6, 0x81, 0x79, 0x3a, 0x00, 0x00, 0x00, 0x00
};

struct t_gsswrapex_data T_GSSWRAPEXv1 = {
    (
      NTLMSSP_NEGOTIATE_56 |
      NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL |
      NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY
    ),
    {
      .data = T_GSSWRAPEXv1_Plaintext_data,
      .length = sizeof(T_GSSWRAPEXv1_Plaintext_data)
    },
    {
      .data = {
        0xeb, 0x93, 0x42, 0x9a, 0x8b, 0xd9, 0x52, 0xf8,
        0xb8, 0x9c, 0x55, 0xb8, 0x7f, 0x47, 0x5e, 0xdc
      },
      .length = 16
    },
    {
      .data = {
        0x04, 0xdd, 0x7f, 0x01, 0x4d, 0x85, 0x04, 0xd2,
        0x65, 0xa2, 0x5c, 0xc8, 0x6a, 0x3a, 0x7c, 0x06
      },
      .length = 16
    },
    {
      .data = {
        0x60, 0xe7, 0x99, 0xbe, 0x5c, 0x72, 0xfc, 0x92,
        0x92, 0x2a, 0xe8, 0xeb, 0xe9, 0x61, 0xfb, 0x8d
      },
      .length = 16
    },
    {
      .data = T_GSSWRAPEXv1_Ciphertext_data,
      .length = sizeof(T_GSSWRAPEXv1_Ciphertext_data)
    },
    {
      .data = T_GSSWRAPEXv1_Signature_data,
      .length = sizeof(T_GSSWRAPEXv1_Signature_data)
    },
};

/* GSS_WrapEx V2 Test Data */

uint8_t T_GSSWRAPEXv2_Plaintext_data[18] = {
    0x50, 0x00, 0x6c, 0x00, 0x61, 0x00, 0x69, 0x00, 0x6e,
    0x00, 0x74, 0x00, 0x65, 0x00, 0x78, 0x00, 0x74, 0x00
};
uint8_t T_GSSWRAPEXv2_Ciphertext_data[18] = {
    0x54, 0xe5, 0x01, 0x65, 0xbf, 0x19, 0x36, 0xdc, 0x99,
    0x60, 0x20, 0xc1, 0x81, 0x1b, 0x0f, 0x06, 0xfb, 0x5f
};
uint8_t T_GSSWRAPEXv2_Signature_data[16] = {
    0x01, 0x00, 0x00, 0x00, 0x7f, 0xb3, 0x8e, 0xc5,
    0xc5, 0x5d, 0x49, 0x76, 0x00, 0x00, 0x00, 0x00
};

struct t_gsswrapex_data T_GSSWRAPEXv2 = {
    (
      NTLMSSP_NEGOTIATE_56 | NTLMSSP_NEGOTIATE_128 |
      NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL |
      NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY
    ),
    {
      .data = T_GSSWRAPEXv2_Plaintext_data,
      .length = sizeof(T_GSSWRAPEXv2_Plaintext_data)
    },
    {
      .data = {
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
    },
      .length = 16
    },
    {
      .data = {
        0x59, 0xf6, 0x00, 0x97, 0x3c, 0xc4, 0x96, 0x0a,
        0x25, 0x48, 0x0a, 0x7c, 0x19, 0x6e, 0x4c, 0x58
      },
      .length = 16
    },
    {
      .data = {
        0x47, 0x88, 0xdc, 0x86, 0x1b, 0x47, 0x82, 0xf3,
        0x5d, 0x43, 0xfd, 0x98, 0xfe, 0x1a, 0x2d, 0x39
      },
      .length = 16
    },
    {
      .data = T_GSSWRAPEXv2_Ciphertext_data,
      .length = sizeof(T_GSSWRAPEXv2_Ciphertext_data)
    },
    {
      .data = T_GSSWRAPEXv2_Signature_data,
      .length = sizeof(T_GSSWRAPEXv2_Signature_data)
    },
};
int test_LMOWFv1(struct ntlm_ctx *ctx)
{
    struct ntlm_key result = { .length = 16 };
    int ret;

    ret = LMOWFv1(T_Passwd, &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv1.ResponseKeyLM.data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv1.ResponseKeyLM.data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    return ret;
}

int test_NTOWFv1(struct ntlm_ctx *ctx)
{
    struct ntlm_key result = { .length = 16 };
    int ret;

    ret = NTOWFv1(T_Passwd, &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv1.ResponseKeyNT.data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv1.ResponseKeyNT.data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    return ret;
}

int test_SessionBaseKeyV1(struct ntlm_ctx *ctx)
{
    struct ntlm_key session_base_key = { .length = 16 };
    int ret;

    ret = ntlm_session_base_key(&T_NTLMv1.ResponseKeyNT, &session_base_key);
    if (ret) return ret;

    if (memcmp(session_base_key.data, T_NTLMv1.SessionBaseKey.data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv1.SessionBaseKey.data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(session_base_key.data));
        ret = EINVAL;
    }

    return ret;
}

int test_LMResponseV1(struct ntlm_ctx *ctx)
{
    uint8_t buf[24];
    struct ntlm_buffer result = { buf, 24 };
    int ret;

    ret = ntlm_compute_lm_response(&T_NTLMv1.ResponseKeyLM, false,
                                   T_ServerChallenge, T_ClientChallenge,
                                   &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv1.LMv1Response, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv1.LMv1Response));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    return ret;
}

int test_NTResponseV1(struct ntlm_ctx *ctx)
{
    uint8_t buf[24];
    struct ntlm_buffer result = { buf, 24 };
    int ret;

    ret = ntlm_compute_nt_response(&T_NTLMv1.ResponseKeyNT, false,
                                   T_ServerChallenge, T_ClientChallenge,
                                   &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv1.NTLMv1Response, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv1.NTLMv1Response));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    return ret;
}

int test_NTOWFv2(struct ntlm_ctx *ctx)
{
    struct ntlm_key nt_hash = { .length = 16 };
    struct ntlm_key result = { .length = 16 };
    int ret;

    ret = NTOWFv1(T_Passwd, &nt_hash);
    if (ret) return ret;

    ret = NTOWFv2(ctx, &nt_hash, T_User, T_UserDom, &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv2.ResponseKeyNT.data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv2.ResponseKeyNT.data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    return ret;
}

int test_LMResponseV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer result;
    int ret;

    ret = ntlmv2_compute_lm_response(&T_NTLMv2.ResponseKeyNT,
                                     T_ServerChallenge, T_ClientChallenge,
                                     &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv2.LMv2Response, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv2.LMv2Response));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    free(result.data);
    return ret;
}

int test_NTResponseV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer target_info = { T_NTLMv2.TargetInfo, 36 };
    struct ntlm_buffer result;
    int ret;

    ret = ntlmv2_compute_nt_response(&T_NTLMv2.ResponseKeyNT,
                                     T_ServerChallenge, T_ClientChallenge,
                                     T_time, &target_info, &result);
    if (ret) return ret;

    if (memcmp(result.data, T_NTLMv2.NTLMv2Response, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv2.NTLMv2Response));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(result.data));
        ret = EINVAL;
    }

    free(result.data);
    return ret;
}

int test_SessionBaseKeyV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer nt_response = { T_NTLMv2.NTLMv2Response, 16 };
    struct ntlm_key session_base_key = { .length = 16 };
    int ret;

    ret = ntlmv2_session_base_key(&T_NTLMv2.ResponseKeyNT,
                                  &nt_response, &session_base_key);
    if (ret) return ret;

    if (memcmp(session_base_key.data, T_NTLMv2.SessionBaseKey.data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(T_NTLMv2.SessionBaseKey.data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(session_base_key.data));
        ret = EINVAL;
    }

    return ret;
}

int test_EncryptedSessionKey(struct ntlm_ctx *ctx,
                             struct ntlm_key *key_exchange_key,
                             struct ntlm_key *encrypted_session_key)
{
    struct ntlm_key exported_session_key = { .length = 16 };
    struct ntlm_key encrypted_random_session_key = { .length = 16 };
    int ret;

    memcpy(exported_session_key.data, T_RandomSessionKey, 16);

    ret = ntlm_encrypted_session_key(key_exchange_key,
                                     &exported_session_key,
                                     &encrypted_random_session_key);
    if (ret) return ret;

    if (memcmp(encrypted_random_session_key.data,
               encrypted_session_key->data, 16) != 0) {
        fprintf(stderr, "results differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_16(encrypted_session_key->data));
        fprintf(stderr, "obtained %s\n",
                        hex_to_str_16(encrypted_random_session_key.data));
        ret = EINVAL;
    }

    return ret;
}

int test_EncryptedSessionKey1(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer lm_response = { T_NTLMv1.LMv1Response, 24 };
    struct ntlm_key key_exchnage_key = { .length = 16 };
    int ret;

    ret = KXKEY(ctx, false, false, false, T_ServerChallenge,
                &T_NTLMv1.ResponseKeyLM, &T_NTLMv1.SessionBaseKey,
                &lm_response, &key_exchnage_key);
    if (ret) return ret;

    return test_EncryptedSessionKey(ctx, &key_exchnage_key,
                                    &T_NTLMv1.EncryptedSessionKey1);
}

int test_EncryptedSessionKey2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer lm_response = { T_NTLMv1.LMv1Response, 24 };
    struct ntlm_key key_exchnage_key = { .length = 16 };
    int ret;

    ret = KXKEY(ctx, false, false, true, T_ServerChallenge,
                &T_NTLMv1.ResponseKeyLM, &T_NTLMv1.SessionBaseKey,
                &lm_response, &key_exchnage_key);
    if (ret) return ret;

    return test_EncryptedSessionKey(ctx, &key_exchnage_key,
                                    &T_NTLMv1.EncryptedSessionKey2);
}

int test_EncryptedSessionKey3(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer lm_response = { T_NTLMv1.LMv1Response, 24 };
    struct ntlm_key key_exchnage_key = { .length = 16 };
    int ret;

    ret = KXKEY(ctx, false, true, false, T_ServerChallenge,
                &T_NTLMv1.ResponseKeyLM, &T_NTLMv1.SessionBaseKey,
                &lm_response, &key_exchnage_key);
    if (ret) return ret;

    return test_EncryptedSessionKey(ctx, &key_exchnage_key,
                                    &T_NTLMv1.EncryptedSessionKey3);
}

int test_DecodeChallengeMessageV1(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer chal_msg = { T_NTLMv1.ChallengeMessage, 0x68 };
    uint32_t type;
    uint32_t flags;
    char *target_name = NULL;
    uint8_t chal[8];
    struct ntlm_buffer challenge = { chal, 8 };
    int ret;

    ret = ntlm_decode_msg_type(ctx, &chal_msg, &type);
    if (ret) return ret;
    if (type != 2) return EINVAL;

    ret = ntlm_decode_chal_msg(ctx, &chal_msg, &flags, &target_name,
                               &challenge, NULL);
    if (ret) return ret;

    if (flags != T_NTLMv1.ChallengeFlags) {
        fprintf(stderr, "flags differ!\n");
        fprintf(stderr, "expected %d\n", T_NTLMv1.ChallengeFlags);
        fprintf(stderr, "obtained %d\n", flags);
        ret = EINVAL;
    }

    if (strcmp(target_name, T_Server_Name) != 0) {
        fprintf(stderr, "Target Names differ!\n");
        fprintf(stderr, "expected %s\n", T_Server_Name);
        fprintf(stderr, "obtained %s\n", target_name);
        ret = EINVAL;
    }

    if (memcmp(chal, T_ServerChallenge, 8) != 0) {
        fprintf(stderr, "Challenges differ!\n");
        fprintf(stderr, "expected %s\n", hex_to_str_8(T_ServerChallenge));
        fprintf(stderr, "obtained %s\n", hex_to_str_8(chal));
        ret = EINVAL;
    }

    free(target_name);
    return ret;
}

int test_EncodeChallengeMessageV1(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer challenge = { T_ServerChallenge, 8 };
    struct ntlm_buffer message = { 0 };
    int ret;

    ret = ntlm_encode_chal_msg(ctx, T_NTLMv1.ChallengeFlags, T_Server_Name,
                               &challenge, NULL, &message);
    if (ret) return ret;

    if ((message.length != 0x44) ||
        (memcmp(message.data, T_NTLMv1.ChallengeMessage,
                              sizeof(T_NTLMv1.ChallengeMessage)) != 0)) {
        fprintf(stderr, "Challenge Messages differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv1.ChallengeMessage,
                                    sizeof(T_NTLMv1.ChallengeMessage)));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(message.data, message.length));
        ret = EINVAL;
    }

    free(message.data);
    return ret;
}

int test_DecodeChallengeMessageV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer chal_msg = { T_NTLMv2.ChallengeMessage,
                                    sizeof(T_NTLMv2.ChallengeMessage) };
    uint32_t type;
    uint32_t flags;
    char *target_name = NULL;
    uint8_t chal[8];
    struct ntlm_buffer challenge = { chal, 8 };
    struct ntlm_buffer target_info = { 0 };
    int ret;

    ret = ntlm_decode_msg_type(ctx, &chal_msg, &type);
    if (ret) return ret;
    if (type != 2) return EINVAL;

    ret = ntlm_decode_chal_msg(ctx, &chal_msg, &flags, &target_name,
                               &challenge, &target_info);
    if (ret) return ret;

    if (flags != T_NTLMv2.ChallengeFlags) {
        fprintf(stderr, "flags differ!\n");
        fprintf(stderr, "expected %d\n", T_NTLMv2.ChallengeFlags);
        fprintf(stderr, "obtained %d\n", flags);
        ret = EINVAL;
    }

    if (strcmp(target_name, T_Server_Name) != 0) {
        fprintf(stderr, "Target Names differ!\n");
        fprintf(stderr, "expected %s\n", T_Server_Name);
        fprintf(stderr, "obtained %s\n", target_name);
        ret = EINVAL;
    }

    if (memcmp(chal, T_ServerChallenge, 8) != 0) {
        fprintf(stderr, "Challenges differ!\n");
        fprintf(stderr, "expected %s\n", hex_to_str_8(T_ServerChallenge));
        fprintf(stderr, "obtained %s\n", hex_to_str_8(chal));
        ret = EINVAL;
    }

    if ((target_info.length != 36) ||
        (memcmp(target_info.data, T_NTLMv2.TargetInfo, 36) != 0)) {
        fprintf(stderr, "Target Infos differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2.TargetInfo, 36));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(target_info.data, target_info.length));
        ret = EINVAL;
    }

    free(target_name);
    free(target_info.data);
    return ret;
}

int test_EncodeChallengeMessageV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer challenge = { T_ServerChallenge, 8 };
    struct ntlm_buffer target_info = { T_NTLMv2.TargetInfo, 36 };
    struct ntlm_buffer message = { 0 };
    int ret;

    ret = ntlm_encode_chal_msg(ctx, T_NTLMv2.ChallengeFlags, T_Server_Name,
                               &challenge, &target_info, &message);
    if (ret) return ret;

    if ((message.length != sizeof(T_NTLMv2.ChallengeMessage)) ||
        (memcmp(message.data, T_NTLMv2.ChallengeMessage,
                              sizeof(T_NTLMv2.ChallengeMessage)) != 0)) {
        fprintf(stderr, "Challenge Messages differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2.ChallengeMessage,
                                    sizeof(T_NTLMv2.ChallengeMessage)));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(message.data, message.length));
        ret = EINVAL;
    }

    free(message.data);
    return ret;
}

int test_DecodeAuthenticateMessageV2(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer auth_msg = { T_NTLMv2.AuthenticateMessage, 0xE8 };
    uint32_t type;
    struct ntlm_buffer lm_chalresp = { 0 };
    struct ntlm_buffer nt_chalresp = { 0 };
    char *dom = NULL;
    char *usr = NULL;
    char *wks = NULL;
    struct ntlm_buffer enc_sess_key = { 0 };
    int ret;

    ret = ntlm_decode_msg_type(ctx, &auth_msg, &type);
    if (ret) return ret;
    if (type != 3) return EINVAL;

    ret = ntlm_decode_auth_msg(ctx, &auth_msg, T_NTLMv2.ChallengeFlags,
                               &lm_chalresp, &nt_chalresp,
                               &dom, &usr, &wks,
                               &enc_sess_key, NULL, NULL);
    if (ret) return ret;

    if ((lm_chalresp.length != 24) ||
        (memcmp(lm_chalresp.data, T_NTLMv2.LMv2Response, 16) != 0)) {

        fprintf(stderr, "LM Challenges differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2.LMv2Response, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(lm_chalresp.data, lm_chalresp.length));
        ret = EINVAL;
    }

    if ((nt_chalresp.length != 84) ||
        (memcmp(nt_chalresp.data, T_NTLMv2.NTLMv2Response, 16) != 0)) {

        fprintf(stderr, "NT Challenges differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2.NTLMv2Response, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(nt_chalresp.data, nt_chalresp.length));
        ret = EINVAL;
    }

    if (strcmp(dom, T_UserDom) != 0) {
        fprintf(stderr, "Domain Names differ!\n");
        fprintf(stderr, "expected %s\n", T_UserDom);
        fprintf(stderr, "obtained %s\n", dom);
        ret = EINVAL;
    }

    if (strcmp(usr, T_User) != 0) {
        fprintf(stderr, "User Names differ!\n");
        fprintf(stderr, "expected %s\n", T_User);
        fprintf(stderr, "obtained %s\n", usr);
        ret = EINVAL;
    }

    if (strcmp(wks, T_Workstation) != 0) {
        fprintf(stderr, "Workstation Names differ!\n");
        fprintf(stderr, "expected %s\n", T_Workstation);
        fprintf(stderr, "obtained %s\n", wks);
        ret = EINVAL;
    }

    if ((enc_sess_key.length != 16) ||
        (memcmp(enc_sess_key.data,
                T_NTLMv2.EncryptedSessionKey.data, 16) != 0)) {

        fprintf(stderr, "EncryptedSessionKey differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2.EncryptedSessionKey.data, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(enc_sess_key.data, enc_sess_key.length));
        ret = EINVAL;
    }

    free(lm_chalresp.data);
    free(nt_chalresp.data);
    free(dom);
    free(usr);
    free(wks);
    free(enc_sess_key.data);
    return ret;
}

int test_EncodeAuthenticateMessageV2(struct ntlm_ctx *ctx)
{
    int ret = 0;






    return ret;
}

int test_DecodeChallengeMessageV2CBT(struct ntlm_ctx *ctx)

{
    struct ntlm_buffer chal_msg = { T_NTLMv2_CBT.ChallengeMessage,
                                    sizeof(T_NTLMv2_CBT.ChallengeMessage) };
    uint32_t type;
    uint32_t flags;
    char *target_name = NULL;
    uint8_t chal[8];
    struct ntlm_buffer challenge = { chal, 8 };
    struct ntlm_buffer target_info = { 0 };
    int ret;

    ret = ntlm_decode_msg_type(ctx, &chal_msg, &type);
    if (ret) return ret;
    if (type != 2) return EINVAL;

    ret = ntlm_decode_chal_msg(ctx, &chal_msg, &flags, &target_name,
                               &challenge, &target_info);
    if (ret) return ret;

    if (flags != T_NTLMv2_CBT.ChallengeFlags) {
        fprintf(stderr, "flags differ!\n");
        fprintf(stderr, "expected 0x%x\n", T_NTLMv2_CBT.ChallengeFlags);
        fprintf(stderr, "obtained 0x%x\n", flags);
        ret = EINVAL;
    }

    if (strcmp(target_name, T_NTLMv2_CBT.Domain) != 0) {
        fprintf(stderr, "Target Names differ!\n");
        fprintf(stderr, "expected %s\n", T_NTLMv2_CBT.Server);
        fprintf(stderr, "obtained %s\n", target_name);
        ret = EINVAL;
    }

    if (memcmp(chal, T_NTLMv2_CBT.ServerChallenge, 8) != 0) {
        fprintf(stderr, "Challenges differ!\n");
        fprintf(stderr, "expected %s\n",
                        hex_to_str_8(T_NTLMv2_CBT.ServerChallenge));
        fprintf(stderr, "obtained %s\n", hex_to_str_8(chal));
        ret = EINVAL;
    }

    if ((target_info.length != sizeof(T_NTLMv2_CBT.TargetInfo)) ||
        (memcmp(target_info.data, T_NTLMv2_CBT.TargetInfo,
                                  sizeof(T_NTLMv2_CBT.TargetInfo)) != 0)) {
        fprintf(stderr, "Target Infos differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2_CBT.TargetInfo,
                                    sizeof(T_NTLMv2_CBT.TargetInfo)));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(target_info.data, target_info.length));
        ret = EINVAL;
    }

    free(target_name);
    free(target_info.data);
    return ret;
}

int test_EncodeChallengeMessageV2CBT(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer challenge = { T_NTLMv2_CBT.ServerChallenge, 8 };
    struct ntlm_buffer target_info = { T_NTLMv2_CBT.TargetInfo,
                                       sizeof(T_NTLMv2_CBT.TargetInfo) };
    struct ntlm_buffer message = { 0 };
    int ret;

    ret = ntlm_encode_chal_msg(ctx, T_NTLMv2_CBT.ChallengeFlags,
                               T_NTLMv2_CBT.Domain, &challenge,
                               &target_info, &message);
    if (ret) return ret;

    if ((message.length != sizeof(T_NTLMv2_CBT.ChallengeMessage)) ||
        (memcmp(message.data, T_NTLMv2_CBT.ChallengeMessage,
                              sizeof(T_NTLMv2_CBT.ChallengeMessage)) != 0)) {
        fprintf(stderr, "Challenge Messages differ!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2_CBT.ChallengeMessage,
                                    sizeof(T_NTLMv2_CBT.ChallengeMessage)));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(message.data, message.length));
        ret = EINVAL;
    }

    free(message.data);
    return ret;
}

int test_DecodeAuthenticateMessageV2CBT(struct ntlm_ctx *ctx)
{
    struct ntlm_buffer auth_msg = { T_NTLMv2_CBT.AuthenticateMessage,
                                    sizeof(T_NTLMv2_CBT.AuthenticateMessage) };
    uint32_t type;
    struct ntlm_buffer lm_chalresp = { 0 };
    struct ntlm_buffer nt_chalresp = { 0 };
    char *dom = NULL;
    char *usr = NULL;
    char *wks = NULL;
    struct ntlm_buffer enc_sess_key = { 0 };
    uint8_t micdata[16];
    struct ntlm_buffer mic = { micdata, 16 };
    struct ntlm_key ntlmv2_key = { .length = 16 };
    struct ntlm_buffer target_info = { 0 };
    struct ntlm_buffer cb = { 0 };
    int ret, c;

    ret = ntlm_decode_msg_type(ctx, &auth_msg, &type);
    if (ret) return ret;
    if (type != 3) return EINVAL;

    ret = ntlm_decode_auth_msg(ctx, &auth_msg, T_NTLMv2_CBT.ChallengeFlags,
                               &lm_chalresp, &nt_chalresp,
                               &dom, &usr, &wks,
                               &enc_sess_key, &target_info, &mic);
    if (ret) return ret;

    for (c = 1; lm_chalresp.length > c; c++) {
        lm_chalresp.data[0] |= lm_chalresp.data[c];
    }
    if ((lm_chalresp.length != 24) || (lm_chalresp.data[0] != 0)) {
        fprintf(stderr, "LM Challenge too short[%zd] or not all zeros!\n",
                        lm_chalresp.length);
        ret = EINVAL;
    }

    if (strcmp(dom, T_NTLMv2_CBT.Domain) != 0) {
        fprintf(stderr, "Domain Names differ!\n");
        fprintf(stderr, "expected %s\n", T_NTLMv2_CBT.Domain);
        fprintf(stderr, "obtained %s\n", dom);
        ret = EINVAL;
    }

    if (strcmp(usr, T_NTLMv2_CBT.User) != 0) {
        fprintf(stderr, "User Names differ!\n");
        fprintf(stderr, "expected %s\n", T_NTLMv2_CBT.User);
        fprintf(stderr, "obtained %s\n", usr);
        ret = EINVAL;
    }

    if (strcmp(wks, T_NTLMv2_CBT.Workstation) != 0) {
        fprintf(stderr, "Workstation Names differ!\n");
        fprintf(stderr, "expected %s\n", T_NTLMv2_CBT.Workstation);
        fprintf(stderr, "obtained %s\n", wks);
        ret = EINVAL;
    }

    if (enc_sess_key.length != 0) {
        fprintf(stderr, "Encrypted Random Session Key not null (%zd)!\n",
                        enc_sess_key.length);
        ret = EINVAL;
    }

    if ((mic.length != 16) ||
        (memcmp(mic.data, T_NTLMv2_CBT.MIC, 16) != 0)) {

        fprintf(stderr, "MIC differs!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2_CBT.MIC, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(mic.data, mic.length));
        ret = EINVAL;
    }

    ret = NTOWFv2(ctx, &T_NTLMv2_CBT.NTLMHash,
                  T_NTLMv2_CBT.User, T_NTLMv2_CBT.Domain,
                  &ntlmv2_key);
    if (ret) {
        fprintf(stderr, "NTLMv2 key generation failed!\n");
        goto done;
    }

    ret = ntlmv2_verify_nt_response(&nt_chalresp, &ntlmv2_key,
                                    T_NTLMv2_CBT.ServerChallenge);
    if (ret) {
        fprintf(stderr, "NTLMv2 Verification failed!\n");
    }

    ret = ntlm_decode_target_info(ctx, &target_info,
                                  NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL,
                                  NULL, &cb);
    if (ret) {
        fprintf(stderr, "NTLMv2 ifailed to decode target info!\n");
    }

    if ((cb.length != 16) ||
        (memcmp(cb.data, T_NTLMv2_CBT.CBSum, 16) != 0)) {
        fprintf(stderr, "CBTs differs!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(T_NTLMv2_CBT.CBSum, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(cb.data, cb.length));
        ret = EINVAL;
    }

done:
    free(lm_chalresp.data);
    free(nt_chalresp.data);
    free(dom);
    free(usr);
    free(wks);
    free(enc_sess_key.data);
    free(target_info.data);
    return ret;
}


int test_GSS_Wrap_EX(struct ntlm_ctx *ctx, struct t_gsswrapex_data *data)
{
    struct ntlm_signseal_state state;
    uint8_t outbuf[data->Ciphertext.length];
    uint8_t signbuf[16];
    struct ntlm_buffer output = { outbuf, data->Ciphertext.length };
    struct ntlm_buffer signature = { signbuf, 16 };
    int ret;

    ret = ntlm_signseal_keys(data->flags, true,
                             &data->KeyExchangeKey, &state);
    if (ret) return ret;

    if (data->ClientSealKey.length) {
        if (memcmp(state.send.seal_key.data,
                   data->ClientSealKey.data,
                   data->ClientSealKey.length) != 0) {
            fprintf(stderr, "Client Sealing Keys differ!\n");
            fprintf(stderr, "expected:\n%s",
                    hex_to_dump(data->ClientSealKey.data,
                                data->ClientSealKey.length));
            fprintf(stderr, "obtained:\n%s",
                    hex_to_dump(state.send.seal_key.data,
                                state.send.seal_key.length));
            ret = EINVAL;
        }
    }

    if (data->ClientSignKey.length) {
        if (memcmp(state.send.sign_key.data,
                   data->ClientSignKey.data,
                   data->ClientSignKey.length) != 0) {
            fprintf(stderr, "Client Signing Keys differ!\n");
            fprintf(stderr, "expected:\n%s",
                    hex_to_dump(data->ClientSignKey.data,
                                data->ClientSignKey.length));
            fprintf(stderr, "obtained:\n%s",
                    hex_to_dump(state.send.sign_key.data,
                                state.send.sign_key.length));
            ret = EINVAL;
        }
    }

    if (ret) return ret;

    ret = ntlm_seal(data->flags, &state,
                    &data->Plaintext, &output, &signature);

    if (ret) {
        fprintf(stderr, "Sealing failed\n");
        return ret;
    }

    if (memcmp(output.data, data->Ciphertext.data,
                            data->Ciphertext.length) != 0) {
        fprintf(stderr, "Ciphertext differs!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(data->Ciphertext.data,
                                    data->Ciphertext.length));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(output.data, output.length));
        ret = EINVAL;
    }

    if (memcmp(signature.data, data->Signature.data, 16) != 0) {
        fprintf(stderr, "Signature differs!\n");
        fprintf(stderr, "expected:\n%s",
                        hex_to_dump(data->Signature.data, 16));
        fprintf(stderr, "obtained:\n%s",
                        hex_to_dump(signature.data, signature.length));
        ret = EINVAL;
    }

    return ret;
}

#define TEST_USER_FILE "examples/test_user_file.txt"

long seed = 0;
static size_t repeatable_rand(uint8_t *buf, size_t max)
{
    char *env_seed;
    size_t len;
    int i;

    if (seed == 0) {
        env_seed = getenv("NTLMSSPTEST_SEED");
        if (env_seed) {
            seed = strtol(env_seed, NULL, 0);
        } else {
            seed = time(NULL);
            fprintf(stdout, "repeatable_rand seed = %ld\n", seed);
        }
        srandom(seed);
    }

    len = random() % max;
    if (len < 5) len = 5;

    for (i = 0; i < len; i++) {
        buf[i] = random();
    }

    return len;
}

int test_gssapi_1(bool user_env_file, bool use_cb)
{
    gss_ctx_id_t cli_ctx = GSS_C_NO_CONTEXT;
    gss_ctx_id_t srv_ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc cli_token = { 0 };
    gss_buffer_desc srv_token = { 0 };
    gss_cred_id_t cli_cred = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t srv_cred = GSS_C_NO_CREDENTIAL;
    const char *username;
    const char *password = "testpassword";
    const char *srvname = "test@testserver";
    gss_name_t gss_username = NULL;
    gss_name_t gss_srvname = NULL;
    gss_buffer_desc pwbuf;
    gss_buffer_desc nbuf;
    uint32_t retmin, retmaj;
    const char *msg = "Sample, signature checking, message.";
    gss_buffer_desc message = { strlen(msg), discard_const(msg) };
    gss_buffer_desc ctx_token;
    uint8_t rand_cb[128];
    struct gss_channel_bindings_struct cbts = { 0 };
    gss_channel_bindings_t cbt = GSS_C_NO_CHANNEL_BINDINGS;
    int ret;

    setenv("NTLM_USER_FILE", TEST_USER_FILE, 0);

    if (user_env_file) {
        username = "testuser";
    } else {
        username = "TESTDOM\\testuser";
    }

    nbuf.value = discard_const(username);
    nbuf.length = strlen(username);
    retmaj = gssntlm_import_name(&retmin, &nbuf,
                                 GSS_C_NT_USER_NAME,
                                 &gss_username);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_name(username) failed! (%d, %s)",
                        retmin, strerror(retmin));
        return EINVAL;
    }

    if (user_env_file) {
        retmaj = gssntlm_acquire_cred(&retmin, (gss_name_t)gss_username,
                                      GSS_C_INDEFINITE, GSS_C_NO_OID_SET,
                                      GSS_C_INITIATE, &cli_cred, NULL, NULL);
        if (retmaj != GSS_S_COMPLETE) {
            fprintf(stderr, "gssntlm_acquire_cred(username) failed! (%d/%d, %s)",
                            retmaj, retmin, strerror(retmin));
            ret = EINVAL;
            goto done;
        }
    } else {
        pwbuf.value = discard_const(password);
        pwbuf.length = strlen(password);
        retmaj = gssntlm_acquire_cred_with_password(&retmin,
                                                    (gss_name_t)gss_username,
                                                    (gss_buffer_t)&pwbuf,
                                                    GSS_C_INDEFINITE,
                                                    GSS_C_NO_OID_SET,
                                                    GSS_C_INITIATE,
                                                    &cli_cred, NULL, NULL);
        if (retmaj != GSS_S_COMPLETE) {
            fprintf(stderr, "gssntlm_acquire_cred_with_password failed! (%d/%d, %s)",
                            retmaj, retmin, strerror(retmin));
            ret = EINVAL;
            goto done;
        }
    }

    retmaj = gssntlm_inquire_cred_by_mech(&retmin, cli_cred, GSS_C_NO_OID,
                                          NULL, NULL, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_cred_by_mech failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        return EINVAL;
    }

    nbuf.value = discard_const(srvname);
    nbuf.length = strlen(srvname);
    retmaj = gssntlm_import_name(&retmin, &nbuf,
                                 GSS_C_NT_HOSTBASED_SERVICE,
                                 &gss_srvname);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_name(srvname) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        return EINVAL;
    }

    retmaj = gssntlm_acquire_cred(&retmin, (gss_name_t)gss_srvname,
                                  GSS_C_INDEFINITE, GSS_C_NO_OID_SET,
                                  GSS_C_ACCEPT, &srv_cred, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_acquire_cred(srvname) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    if (use_cb) {
        /* generate random cb */
        cbts.application_data.length = repeatable_rand(rand_cb, 128);
        cbts.application_data.value = rand_cb;
        cbt = &cbts;
    }

    retmaj = gssntlm_init_sec_context(&retmin, cli_cred, &cli_ctx,
                                      gss_srvname, GSS_C_NO_OID,
                                      GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG,
                                      0, cbt,
                                      GSS_C_NO_BUFFER, NULL, &cli_token,
                                      NULL, NULL);
    if (retmaj != GSS_S_CONTINUE_NEEDED) {
        fprintf(stderr, "gssntlm_init_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_accept_sec_context(&retmin, &srv_ctx, srv_cred,
                                        &cli_token, cbt,
                                        NULL, NULL, &srv_token,
                                        NULL, NULL, NULL);
    if (retmaj != GSS_S_CONTINUE_NEEDED) {
        fprintf(stderr, "gssntlm_accept_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);

    /* test importing and exporting context before it is fully estabished */
    retmaj = gssntlm_export_sec_context(&retmin, &srv_ctx, &ctx_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_export_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }
    retmaj = gssntlm_import_sec_context(&retmin, &ctx_token, &srv_ctx);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }
    gss_release_buffer(&retmin, &ctx_token);

    retmaj = gssntlm_init_sec_context(&retmin, cli_cred, &cli_ctx,
                                      gss_srvname, GSS_C_NO_OID,
                                      GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG,
                                      0, cbt,
                                      &srv_token, NULL, &cli_token,
                                      NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_init_sec_context 2 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &srv_token);

    retmaj = gssntlm_accept_sec_context(&retmin, &srv_ctx, srv_cred,
                                        &cli_token, cbt,
                                        NULL, NULL, &srv_token,
                                        NULL, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_accept_sec_context 2 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);

    /* test importing and exporting context after it is fully estabished */
    retmaj = gssntlm_export_sec_context(&retmin, &cli_ctx, &ctx_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_export_sec_context 2 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }
    retmaj = gssntlm_import_sec_context(&retmin, &ctx_token, &cli_ctx);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_sec_context 2 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }
    gss_release_buffer(&retmin, &ctx_token);

    retmaj = gssntlm_get_mic(&retmin, cli_ctx, 0, &message, &cli_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_get_mic(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_verify_mic(&retmin, srv_ctx, &message, &cli_token, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_verify_mic(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);

    retmaj = gssntlm_get_mic(&retmin, srv_ctx, 0, &message, &srv_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_get_mic(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_verify_mic(&retmin, cli_ctx, &message, &srv_token, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_verify_mic(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &srv_token);

    retmaj = gssntlm_wrap(&retmin, cli_ctx, 1, 0, &message, NULL, &cli_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_wrap(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_unwrap(&retmin, srv_ctx,
                            &cli_token, &srv_token, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_unwrap(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    if (memcmp(message.value, srv_token.value, srv_token.length) != 0) {
        fprintf(stderr, "sealing and unsealing failed to return the "
                        "same result (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);

    gssntlm_release_name(&retmin, &gss_username);
    gssntlm_release_name(&retmin, &gss_srvname);

    retmaj = gssntlm_inquire_context(&retmin, srv_ctx,
                                     &gss_username, &gss_srvname,
                                     NULL, NULL, NULL, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_inquire_context failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_display_name(&retmin, gss_username, &nbuf, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_display_name failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    if (strcmp(nbuf.value, "TESTDOM\\testuser") != 0) {
        fprintf(stderr, "Expected username of [%s] but got [%s] instead!\n",
                        "TESTDOM\\testuser", (char *)nbuf.value);
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &nbuf);

    ret = 0;

done:
    gssntlm_delete_sec_context(&retmin, &cli_ctx, GSS_C_NO_BUFFER);
    gssntlm_delete_sec_context(&retmin, &srv_ctx, GSS_C_NO_BUFFER);
    gssntlm_release_name(&retmin, &gss_username);
    gssntlm_release_name(&retmin, &gss_srvname);
    gssntlm_release_cred(&retmin, &cli_cred);
    gssntlm_release_cred(&retmin, &srv_cred);
    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);
    return ret;
}

int test_gssapi_cl(void)
{
    gss_ctx_id_t cli_ctx = GSS_C_NO_CONTEXT;
    gss_ctx_id_t srv_ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc cli_token = { 0 };
    gss_buffer_desc srv_token = { 0 };
    gss_cred_id_t cli_cred = GSS_C_NO_CREDENTIAL;
    gss_cred_id_t srv_cred = GSS_C_NO_CREDENTIAL;
    const char *username = "TESTDOM\\testuser";
    const char *password = "testpassword";
    const char *srvname = "test@testserver";
    gss_name_t gss_username = NULL;
    gss_name_t gss_srvname = NULL;
    gss_buffer_desc pwbuf;
    gss_buffer_desc nbuf;
    gss_OID_desc set_seqnum_oid = {
        GSS_NTLMSSP_SET_SEQ_NUM_OID_LENGTH,
        discard_const(GSS_NTLMSSP_SET_SEQ_NUM_OID_STRING)
    };
    gss_buffer_desc set_seqnum_buf;
    uint32_t app_seq_num;
    uint32_t retmin, retmaj;
    const char *msg = "Sample, signature checking, message.";
    gss_buffer_desc message = { strlen(msg), discard_const(msg) };
    int ret;

    setenv("NTLM_USER_FILE", TEST_USER_FILE, 0);

    nbuf.value = discard_const(username);
    nbuf.length = strlen(username);
    retmaj = gssntlm_import_name(&retmin, &nbuf,
                                 GSS_C_NT_USER_NAME,
                                 &gss_username);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_name(username) failed! (%d, %s)",
                        retmin, strerror(retmin));
        return EINVAL;
    }

    pwbuf.value = discard_const(password);
    pwbuf.length = strlen(password);
    retmaj = gssntlm_acquire_cred_with_password(&retmin,
                                                (gss_name_t)gss_username,
                                                (gss_buffer_t)&pwbuf,
                                                GSS_C_INDEFINITE,
                                                GSS_C_NO_OID_SET,
                                                GSS_C_INITIATE,
                                                &cli_cred, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_acquire_cred_with_password failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        return EINVAL;
    }

    nbuf.value = discard_const(srvname);
    nbuf.length = strlen(srvname);
    retmaj = gssntlm_import_name(&retmin, &nbuf,
                                 GSS_C_NT_HOSTBASED_SERVICE,
                                 &gss_srvname);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_import_name(srvname) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        return EINVAL;
    }

    retmaj = gssntlm_acquire_cred(&retmin, (gss_name_t)gss_srvname,
                                  GSS_C_INDEFINITE, GSS_C_NO_OID_SET,
                                  GSS_C_ACCEPT, &srv_cred, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_acquire_cred(srvname) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_accept_sec_context(&retmin, &srv_ctx, srv_cred,
                                        &cli_token, GSS_C_NO_CHANNEL_BINDINGS,
                                        NULL, NULL, &srv_token,
                                        NULL, NULL, NULL);
    if (retmaj != GSS_S_CONTINUE_NEEDED) {
        fprintf(stderr, "gssntlm_accept_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);

    retmaj = gssntlm_init_sec_context(&retmin, cli_cred, &cli_ctx,
                                      gss_srvname, GSS_C_NO_OID,
                                      GSS_C_CONF_FLAG |
                                          GSS_C_INTEG_FLAG |
                                          GSS_C_DATAGRAM_FLAG,
                                      0, GSS_C_NO_CHANNEL_BINDINGS,
                                      &srv_token, NULL, &cli_token,
                                      NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_init_sec_context 1 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &srv_token);

    retmaj = gssntlm_accept_sec_context(&retmin, &srv_ctx, srv_cred,
                                        &cli_token, GSS_C_NO_CHANNEL_BINDINGS,
                                        NULL, NULL, &srv_token,
                                        NULL, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_accept_sec_context 2 failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    /* TODO: again with channel bindings */

    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);

    /* arbitrary seq number forced on the context */
    app_seq_num = 10;
    set_seqnum_buf.value = &app_seq_num;
    set_seqnum_buf.length = 4;
    retmaj = gssntlm_set_sec_context_option(&retmin, (gss_ctx_id_t *)&cli_ctx,
                                            &set_seqnum_oid,
                                            &set_seqnum_buf);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_set_sec_context_option(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_set_sec_context_option(&retmin, (gss_ctx_id_t *)&srv_ctx,
                                            &set_seqnum_oid,
                                            &set_seqnum_buf);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_set_sec_context_option(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_get_mic(&retmin, cli_ctx, 0, &message, &cli_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_get_mic(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_verify_mic(&retmin, srv_ctx, &message, &cli_token, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_verify_mic(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);

    retmaj = gssntlm_get_mic(&retmin, srv_ctx, 0, &message, &srv_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_get_mic(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_verify_mic(&retmin, cli_ctx, &message, &srv_token, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_verify_mic(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &srv_token);

    retmaj = gssntlm_wrap(&retmin, cli_ctx, 1, 0, &message, NULL, &cli_token);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_wrap(cli) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    retmaj = gssntlm_unwrap(&retmin, srv_ctx,
                            &cli_token, &srv_token, NULL, NULL);
    if (retmaj != GSS_S_COMPLETE) {
        fprintf(stderr, "gssntlm_unwrap(srv) failed! (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    if (memcmp(message.value, srv_token.value, srv_token.length) != 0) {
        fprintf(stderr, "sealing and unsealing failed to return the "
                        "same result (%d/%d, %s)",
                        retmaj, retmin, strerror(retmin));
        ret = EINVAL;
        goto done;
    }

    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);

    gssntlm_release_name(&retmin, &gss_username);
    gssntlm_release_name(&retmin, &gss_srvname);

    ret = 0;

done:
    gssntlm_delete_sec_context(&retmin, &cli_ctx, GSS_C_NO_BUFFER);
    gssntlm_delete_sec_context(&retmin, &srv_ctx, GSS_C_NO_BUFFER);
    gssntlm_release_name(&retmin, &gss_username);
    gssntlm_release_name(&retmin, &gss_srvname);
    gssntlm_release_cred(&retmin, &cli_cred);
    gssntlm_release_cred(&retmin, &srv_cred);
    gss_release_buffer(&retmin, &cli_token);
    gss_release_buffer(&retmin, &srv_token);
    return ret;
}

int main(int argc, const char *argv[])
{
    struct ntlm_ctx *ctx;
    int ret;

    ret = ntlm_init_ctx(&ctx);
    if (ret) goto done;

    fprintf(stdout, "Test LMOWFv1\n");
    ret = test_LMOWFv1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test NTOWFv1\n");
    ret = test_NTOWFv1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test LMResponse v1\n");
    ret = test_LMResponseV1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test NTResponse v1\n");
    ret = test_NTResponseV1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test SessionBaseKey v1\n");
    ret = test_SessionBaseKeyV1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test EncryptedSessionKey v1 (1)\n");
    ret = test_EncryptedSessionKey1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test EncryptedSessionKey v1 (2)\n");
    ret = test_EncryptedSessionKey2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test EncryptedSessionKey v1 (3)\n");
    ret = test_EncryptedSessionKey3(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    /* override internal version for V1 test vector */
    ntlm_internal_set_version(6, 0, 6000, 15);

    fprintf(stdout, "Test decoding ChallengeMessage v1\n");
    ret = test_DecodeChallengeMessageV1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test encoding ChallengeMessage v1\n");
    ret = test_EncodeChallengeMessageV1(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test LMResponse v2\n");
    ret = test_LMResponseV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test NTResponse v2\n");
    ret = test_NTResponseV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test SessionBaseKey v2\n");
    ret = test_SessionBaseKeyV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test EncryptedSessionKey v2\n");
    ret = test_EncryptedSessionKey(ctx, &T_NTLMv2.SessionBaseKey,
                                   &T_NTLMv2.EncryptedSessionKey);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    /* override internal version for V2 test vector */
    ntlm_internal_set_version(6, 0, 6000, 15);

    fprintf(stdout, "Test decoding ChallengeMessage v2\n");
    ret = test_DecodeChallengeMessageV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test encoding ChallengeMessage v2\n");
    ret = test_EncodeChallengeMessageV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test decoding AuthenticateMessage v2\n");
    ret = test_DecodeAuthenticateMessageV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test encoding AuthenticateMessage v2\n");
    ret = test_EncodeAuthenticateMessageV2(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    /* override internal version for CBT test vector */
    ntlm_internal_set_version(6, 1, 7600, 15);

    fprintf(stdout, "Test decoding ChallengeMessage v2 with CBT\n");
    ret = test_DecodeChallengeMessageV2CBT(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test encoding ChallengeMessage v2 with CBT\n");
    ret = test_EncodeChallengeMessageV2CBT(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test decoding AuthenticateMessage v2 with CBT\n");
    ret = test_DecodeAuthenticateMessageV2CBT(ctx);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test sealing a Message with No Extended Security\n");
    ret = test_GSS_Wrap_EX(ctx, &T_GSSWRAPv1noESS);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test sealing a Message with NTLMv1 Extended Security\n");
    ret = test_GSS_Wrap_EX(ctx, &T_GSSWRAPEXv1);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test sealing a Message with NTLMv2 Extended Security\n");
    ret = test_GSS_Wrap_EX(ctx, &T_GSSWRAPEXv2);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, " *** Test with NTLMv1 auth");
    setenv("LM_COMPAT_LEVEL", "0", 1);

    fprintf(stdout, "Test GSSAPI conversation (user env file)\n");
    ret = test_gssapi_1(true, false);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test GSSAPI conversation (with password)\n");
    ret = test_gssapi_1(false, false);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test Connectionless exchange\n");
    ret = test_gssapi_cl();
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, " *** Again forcing NTLMv2 auth");
    setenv("LM_COMPAT_LEVEL", "5", 1);

    fprintf(stdout, "Test GSSAPI conversation (user env file)\n");
    ret = test_gssapi_1(true, false);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test GSSAPI conversation (with password)\n");
    ret = test_gssapi_1(false, false);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test GSSAPI conversation (with CB)\n");
    ret = test_gssapi_1(false, true);
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

    fprintf(stdout, "Test Connectionless exchange\n");
    ret = test_gssapi_cl();
    fprintf(stdout, "Test: %s\n", (ret ? "FAIL":"SUCCESS"));

done:
    ntlm_free_ctx(&ctx);
    return ret;
}
