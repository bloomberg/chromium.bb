// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_TEST_UTILS_H_
#define NET_SPDY_TEST_UTILS_H_

#include <stdint.h>

#include <string>

#include "net/spdy/spdy_protocol.h"

namespace net {

class HashValue;
class TransportSecurityState;

namespace test {

std::string HexDumpWithMarks(const unsigned char* data, int length,
                             const bool* marks, int mark_length);

void CompareCharArraysWithHexError(
    const std::string& description,
    const unsigned char* actual,
    const int actual_len,
    const unsigned char* expected,
    const int expected_len);

void SetFrameFlags(SpdyFrame* frame,
                   uint8 flags,
                   SpdyMajorVersion spdy_version);

void SetFrameLength(SpdyFrame* frame,
                    size_t length,
                    SpdyMajorVersion spdy_version);

std::string a2b_hex(const char* hex_data);

// Returns a SHA1 HashValue in which each byte has the value |label|.
HashValue GetTestHashValue(uint8_t label);

// Returns SHA1 pinning header for the of the base64 encoding of
// GetTestHashValue(|label|).
std::string GetTestPin(uint8_t label);

// Adds a pin for |host| to |state|.
void AddPin(TransportSecurityState* state,
            const std::string& host,
            uint8_t primary_label,
            uint8_t backup_label);

}  // namespace test

}  // namespace net

#endif  // NET_SPDY_TEST_UTILS_H_
