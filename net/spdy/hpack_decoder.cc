// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include "base/basictypes.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;
using std::string;

HpackDecoder::HpackDecoder(const HpackHuffmanTable& table,
                           uint32 max_string_literal_size)
    : max_string_literal_size_(max_string_literal_size),
      huffman_table_(table) {}

HpackDecoder::~HpackDecoder() {}

void HpackDecoder::SetMaxHeadersSize(uint32 max_size) {
  context_.SetMaxSize(max_size);
}

bool HpackDecoder::DecodeHeaderSet(StringPiece input,
                                   HpackHeaderPairVector* header_list) {
  HpackInputStream input_stream(max_string_literal_size_, input);
  while (input_stream.HasMoreData()) {
    // May add to |header_list|.
    if (!ProcessNextHeaderRepresentation(&input_stream, header_list))
      return false;
  }

  // After processing opcodes, emit everything in the reference set
  // that hasn't already been emitted.
  for (size_t i = 1; i <= context_.GetMutableEntryCount(); ++i) {
    if (context_.IsReferencedAt(i) &&
        (context_.GetTouchCountAt(i) == HpackEncodingContext::kUntouched)) {
      header_list->push_back(
          HpackHeaderPair(context_.GetNameAt(i).as_string(),
                          context_.GetValueAt(i).as_string()));
    }
    context_.ClearTouchesAt(i);
  }

  return true;
}

bool HpackDecoder::ProcessNextHeaderRepresentation(
    HpackInputStream* input_stream, HpackHeaderPairVector* header_list) {
  // Touches are used below to track which headers have been emitted.

  // Implements 4.2. Indexed Header Field Representation.
  if (input_stream->MatchPrefixAndConsume(kIndexedOpcode)) {
    uint32 index_or_zero = 0;
    if (!input_stream->DecodeNextUint32(&index_or_zero))
      return false;

    if (index_or_zero > context_.GetEntryCount())
      return false;

    bool emitted = false;
    if (index_or_zero > 0) {
      uint32 index = index_or_zero;
      // The index will be put into the reference set.
      if (!context_.IsReferencedAt(index)) {
        header_list->push_back(
            HpackHeaderPair(context_.GetNameAt(index).as_string(),
                            context_.GetValueAt(index).as_string()));
        emitted = true;
      }
    }

    uint32 new_index = 0;
    std::vector<uint32> removed_referenced_indices;
    context_.ProcessIndexedHeader(
        index_or_zero, &new_index, &removed_referenced_indices);

    if (emitted && new_index > 0)
      context_.AddTouchesAt(new_index, 0);

    return true;
  }

  // Implements 4.3.1. Literal Header Field without Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralNoIndexOpcode)) {
    StringPiece name;
    if (!DecodeNextName(input_stream, &name))
      return false;

    StringPiece value;
    if (!DecodeNextStringLiteral(input_stream, false, &value))
      return false;

    header_list->push_back(
        HpackHeaderPair(name.as_string(), value.as_string()));
    return true;
  }

  // Implements 4.3.2. Literal Header Field with Incremental Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralIncrementalIndexOpcode)) {
    StringPiece name;
    if (!DecodeNextName(input_stream, &name))
      return false;

    StringPiece value;
    if (!DecodeNextStringLiteral(input_stream, false, &value))
      return false;

    header_list->push_back(
        HpackHeaderPair(name.as_string(), value.as_string()));

    uint32 new_index = 0;
    std::vector<uint32> removed_referenced_indices;
    context_.ProcessLiteralHeaderWithIncrementalIndexing(
        name, value, &new_index, &removed_referenced_indices);

    if (new_index > 0)
      context_.AddTouchesAt(new_index, 0);

    return true;
  }

  return false;
}

bool HpackDecoder::DecodeNextName(
    HpackInputStream* input_stream, StringPiece* next_name) {
  uint32 index_or_zero = 0;
  if (!input_stream->DecodeNextUint32(&index_or_zero))
    return false;

  if (index_or_zero == 0)
    return DecodeNextStringLiteral(input_stream, true, next_name);

  uint32 index = index_or_zero;
  if (index > context_.GetEntryCount())
    return false;

  *next_name = context_.GetNameAt(index_or_zero);
  return true;
}

bool HpackDecoder::DecodeNextStringLiteral(HpackInputStream* input_stream,
                                           bool is_key,
                                           StringPiece* output) {
  if (input_stream->MatchPrefixAndConsume(kStringLiteralHuffmanEncoded)) {
    string* buffer = is_key ? &huffman_key_buffer_ : &huffman_value_buffer_;
    bool result = input_stream->DecodeNextHuffmanString(huffman_table_, buffer);
    *output = StringPiece(*buffer);
    return result;
  } else if (input_stream->MatchPrefixAndConsume(
      kStringLiteralIdentityEncoded)) {
    return input_stream->DecodeNextIdentityString(output);
  } else {
    return false;
  }
}

}  // namespace net
