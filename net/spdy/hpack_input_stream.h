// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_INPUT_STREAM_H_
#define NET_SPDY_HPACK_INPUT_STREAM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_constants.h"  // For HpackPrefix.

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05
// .

namespace net {

// TODO(akalin): When we use a callback/delegate instead of a vector,
// use StringPiece instead of string.
typedef std::pair<std::string, std::string> HpackHeaderPair;
typedef std::vector<HpackHeaderPair> HpackHeaderPairVector;

// An HpackInputStream handles all the low-level details of decoding
// header fields.
class NET_EXPORT_PRIVATE HpackInputStream {
 public:
  // |max_string_literal_size| is the largest that any one string
  // literal (header name or header value) can be.
  HpackInputStream(uint32 max_string_literal_size, base::StringPiece buffer);
  ~HpackInputStream();

  // Returns whether or not there is more data to process.
  bool HasMoreData() const;

  // If the next octet has the top |size| bits equal to |bits|,
  // consumes it and returns true. Otherwise, consumes nothing and
  // returns false.
  bool MatchPrefixAndConsume(HpackPrefix prefix);

  // The Decode* functions return true and fill in their arguments if
  // decoding was successful, or false if an error was encountered.

  bool DecodeNextUint32(uint32* I);
  bool DecodeNextStringLiteral(base::StringPiece* str);

  // Accessors for testing.

  void SetBitOffsetForTest(size_t bit_offset) {
    bit_offset_ = bit_offset;
  }

  bool DecodeNextUint32ForTest(uint32* I) {
    return DecodeNextUint32(I);
  }

  bool DecodeNextStringLiteralForTest(base::StringPiece *str) {
    return DecodeNextStringLiteral(str);
  }

 private:
  const uint32 max_string_literal_size_;
  base::StringPiece buffer_;
  size_t bit_offset_;

  bool PeekNextOctet(uint8* next_octet);

  bool DecodeNextOctet(uint8* next_octet);

  DISALLOW_COPY_AND_ASSIGN(HpackInputStream);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_INPUT_STREAM_H_
