// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_CONSTANTS_H_
#define NET_SPDY_HPACK_CONSTANTS_H_

#include "base/basictypes.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05
// .

namespace net {

// The marker for a string literal that is stored unmodified (i.e.,
// without Huffman encoding) (from 4.1.2).
const uint8 kStringLiteralIdentityEncoded = 0x0;
const uint8 kStringLiteralIdentityEncodedSize = 1;

// The opcode for a literal header field without indexing (from
// 4.3.1).
const uint8 kLiteralNoIndexOpcode = 0x01;
const uint8 kLiteralNoIndexOpcodeSize = 2;

}  // namespace net

#endif  // NET_SPDY_HPACK_CONSTANTS_H_
