// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_OUTPUT_STREAM_H_
#define NET_SPDY_HPACK_OUTPUT_STREAM_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_constants.h"  // For HpackPrefix.
#include "net/spdy/hpack_encoding_context.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05
// .

namespace net {

// An HpackOutputStream handles all the low-level details of encoding
// header fields.
class NET_EXPORT_PRIVATE HpackOutputStream {
 public:
  // |max_string_literal_size| is the largest that any one string
  // |literal (header name or header value) can be.
  explicit HpackOutputStream(uint32 max_string_literal_size);
  ~HpackOutputStream();

  // Corresponds to 4.2.
  void AppendIndexedHeader(uint32 index_or_zero);

  // Corresponds to 4.3.1 (second form). Returns whether or not the
  // append was successful; if the append was unsuccessful, no other
  // member function may be called.
  bool AppendLiteralHeaderNoIndexingWithName(base::StringPiece name,
                                             base::StringPiece value);

  // Moves the internal buffer to the given string and clears all
  // internal state.
  void TakeString(std::string* output);

  // Accessors for testing.

  void AppendBitsForTest(uint8 bits, size_t size) {
    AppendBits(bits, size);
  }

  void AppendUint32ForTest(uint32 I) {
    AppendUint32(I);
  }

  bool AppendStringLiteralForTest(base::StringPiece str) {
    return AppendStringLiteral(str);
  }

 private:
  // Appends the lower |bit_size| bits of |bits| to the internal buffer.
  //
  // |bit_size| must be > 0 and <= 8. |bits| must not have any bits
  // set other than the lower |bit_size| bits.
  void AppendBits(uint8 bits, size_t bit_size);

  // Simply forwards to AppendBits(prefix.bits, prefix.bit-size).
  void AppendPrefix(HpackPrefix prefix);

  // Appends the given integer using the representation described in
  // 4.1.1. If the internal buffer ends on a byte boundary, the prefix
  // length N is taken to be 8; otherwise, it is taken to be the
  // number of bits to the next byte boundary.
  //
  // It is guaranteed that the internal buffer will end on a byte
  // boundary after this function is called.
  void AppendUint32(uint32 I);

  // Appends the given string using the representation described in
  // 4.1.2. The internal buffer must end on a byte boundary, and it is
  // guaranteed that the internal buffer will end on a byte boundary
  // after this function is called. Returns whether or not the append
  // was successful; if the append was unsuccessful, no other member
  // function may be called.
  bool AppendStringLiteral(base::StringPiece str);

  const uint32 max_string_literal_size_;

  // The internal bit buffer.
  std::string buffer_;

  // If 0, the buffer ends on a byte boundary. If non-zero, the buffer
  // ends on the most significant nth bit. Guaranteed to be < 8.
  size_t bit_offset_;

  DISALLOW_COPY_AND_ASSIGN(HpackOutputStream);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_OUTPUT_STREAM_H_
