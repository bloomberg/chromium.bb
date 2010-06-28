// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_H_
#define NET_SPDY_SPDY_TEST_UTIL_H_

#include "base/basictypes.h"

namespace net {

const uint8 kGetSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x49,                                        // FIN, len
  0x00, 0x00, 0x00, 0x01,                                        // stream id
  0x00, 0x00, 0x00, 0x00,                                        // associated
  0xc0, 0x00, 0x00, 0x03,                                        // 3 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x03, 'G', 'E', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

const uint8 kGetSynReply[] = {
  0x80, 0x01, 0x00, 0x02,                                        // header
  0x00, 0x00, 0x00, 0x45,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x04,                                        // 4 headers
  0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
  0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
  0x00, 0x03, '2', '0', '0',                                     // "200"
  0x00, 0x03, 'u', 'r', 'l',                                     // "url"
  0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "/index...
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
};

const uint8 kGetRst[] = {
  0x80, 0x01, 0x00, 0x03,                                        // header
  0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x05,                                        // 4 headers
};

const uint8 kGetBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

const uint8 kGetSynCompressed[] = {
  0x80, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0x47,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0xc0, 0x00, 0x38, 0xea, 0xdf, 0xa2, 0x51, 0xb2,
  0x62, 0x60, 0x66, 0x60, 0xcb, 0x05, 0xe6, 0xc3,
  0xfc, 0x14, 0x06, 0x66, 0x77, 0xd7, 0x10, 0x06,
  0x66, 0x90, 0xa0, 0x58, 0x46, 0x49, 0x49, 0x81,
  0x95, 0xbe, 0x3e, 0x30, 0xe2, 0xf5, 0xd2, 0xf3,
  0xf3, 0xd3, 0x73, 0x52, 0xf5, 0x92, 0xf3, 0x73,
  0xf5, 0x19, 0xd8, 0xa1, 0x1a, 0x19, 0x38, 0x60,
  0xe6, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

const uint8 kPostSyn[] = {
  0x80, 0x01, 0x00, 0x01,                                      // header
  0x00, 0x00, 0x00, 0x4a,                                      // flags, len
  0x00, 0x00, 0x00, 0x01,                                      // stream id
  0x00, 0x00, 0x00, 0x00,                                      // associated
  0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
  0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
  0x00, 0x04, 'P', 'O', 'S', 'T',
  0x00, 0x03, 'u', 'r', 'l',
  0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
              '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
              'm', '/',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
};

const uint8 kPostUploadFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x0c,                                        // FIN flag
  'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0'
};

// The response
const uint8 kPostSynReply[] = {
  0x80, 0x01, 0x00, 0x02,                                        // header
  0x00, 0x00, 0x00, 0x45,
  0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x04,                                        // 4 headers
  0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
  0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
  0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
  0x00, 0x03, '2', '0', '0',                                     // "200"
  0x00, 0x03, 'u', 'r', 'l',                                     // "url"
  // "/index.php"
  0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',
  0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
  0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
};

const uint8 kPostBodyFrame[] = {
  0x00, 0x00, 0x00, 0x01,                                        // header
  0x01, 0x00, 0x00, 0x06,                                        // FIN, length
  'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
};

const uint8 kGoAway[] = {
  0x80, 0x01, 0x00, 0x07,  // header
  0x00, 0x00, 0x00, 0x04,  // flags, len
  0x00, 0x00, 0x00, 0x00,  // last-accepted-stream-id
};

}  // namespace net

#endif // NET_SPDY_SPDY_TEST_UTIL_H_
