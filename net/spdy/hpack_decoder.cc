// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include "base/basictypes.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;

HpackDecoder::HpackDecoder(uint32 max_string_literal_size)
    : max_string_literal_size_(max_string_literal_size) {}

HpackDecoder::~HpackDecoder() {}

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
  // Implements 4.3.1. Literal Header Field without Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralNoIndexOpcode)) {
    StringPiece name;
    if (!DecodeNextName(input_stream, &name))
      return false;

    StringPiece value;
    if (!DecodeNextValue(input_stream, &value))
      return false;

    header_list->push_back(
        HpackHeaderPair(name.as_string(), value.as_string()));
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
    return input_stream->DecodeNextStringLiteral(next_name);

  uint32 index = index_or_zero;
  if (index > context_.GetEntryCount())
    return false;

  *next_name = context_.GetNameAt(index_or_zero);
  return true;
}

bool HpackDecoder::DecodeNextValue(
    HpackInputStream* input_stream, StringPiece* next_name) {
  return input_stream->DecodeNextStringLiteral(next_name);
}

}  // namespace net
