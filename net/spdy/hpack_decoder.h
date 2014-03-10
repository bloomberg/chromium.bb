// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_DECODER_H_
#define NET_SPDY_HPACK_DECODER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/spdy/hpack_encoding_context.h"
#include "net/spdy/hpack_input_stream.h"

// An HpackDecoder decodes header sets as outlined in
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-06

namespace net {

class HpackHuffmanTable;

namespace test {
class HpackDecoderPeer;
}  // namespace test

class NET_EXPORT_PRIVATE HpackDecoder {
 public:
  friend class test::HpackDecoderPeer;

  // |table| is an initialized HPACK Huffman table, having an
  // externally-managed lifetime which spans beyond HpackDecoder.
  explicit HpackDecoder(const HpackHuffmanTable& table,
                        uint32 max_string_literal_size);
  ~HpackDecoder();

  // Called upon acknowledgement of SETTINGS_HEADER_TABLE_SIZE.
  // See HpackEncodingContext::ApplyHeaderTableSizeSetting().
  void ApplyHeaderTableSizeSetting(uint32 max_size);

  // Decodes the given string into the given header set. Returns
  // whether or not the decoding was successful.
  //
  // TODO(jgraettinger): Parse headers using incrementally-receieved data,
  // and emit headers via a delegate. (See SpdyHeadersBlockParser and
  // SpdyHeadersHandlerInterface).
  bool DecodeHeaderSet(base::StringPiece input,
                       HpackHeaderPairVector* header_list);

  // Accessors for testing.

  bool DecodeNextNameForTest(HpackInputStream* input_stream,
                             base::StringPiece* next_name) {
    return DecodeNextName(input_stream, next_name);
  }

 private:
  const uint32 max_string_literal_size_;
  HpackEncodingContext context_;

  // Huffman table to be applied to decoded Huffman literals,
  // and scratch space for storing those decoded literals.
  const HpackHuffmanTable& huffman_table_;
  std::string huffman_key_buffer_, huffman_value_buffer_;

  // Handlers for decoding HPACK opcodes and header representations
  // (or parts thereof). These methods emit headers into
  // |header_list|, and return true on success and false on error.
  bool DecodeNextOpcode(HpackInputStream* input_stream,
                        HpackHeaderPairVector* header_list);
  bool DecodeNextContextUpdate(HpackInputStream* input_stream);
  bool DecodeNextIndexedHeader(HpackInputStream* input_stream,
                               HpackHeaderPairVector* header_list);
  bool DecodeNextLiteralHeader(HpackInputStream* input_stream,
                               bool should_index,
                               HpackHeaderPairVector* header_list);
  bool DecodeNextName(HpackInputStream* input_stream,
                      base::StringPiece* next_name);
  bool DecodeNextStringLiteral(HpackInputStream* input_stream,
                               bool is_header_key,  // As distinct from a value.
                               base::StringPiece* output);

  DISALLOW_COPY_AND_ASSIGN(HpackDecoder);
};

}  // namespace net

#endif  // NET_SPDY_HPACK_DECODER_H_
