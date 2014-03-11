// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;
using std::string;

HpackDecoder::HpackDecoder(const HpackHuffmanTable& table)
    : max_string_literal_size_(kDefaultMaxStringLiteralSize),
      huffman_table_(table) {}

HpackDecoder::~HpackDecoder() {}

void HpackDecoder::ApplyHeaderTableSizeSetting(uint32 max_size) {
  context_.ApplyHeaderTableSizeSetting(max_size);
}

bool HpackDecoder::HandleControlFrameHeadersData(SpdyStreamId id,
                                                 const char* headers_data,
                                                 size_t headers_data_length) {
  decoded_block_.clear();

  size_t new_size = headers_block_buffer_.size() + headers_data_length;
  if (new_size > kMaxDecodeBufferSize) {
    return false;
  }
  headers_block_buffer_.insert(headers_block_buffer_.end(),
                               headers_data,
                               headers_data + headers_data_length);
  return true;
}

bool HpackDecoder::HandleControlFrameHeadersComplete(SpdyStreamId id) {
  HpackInputStream input_stream(max_string_literal_size_,
                                headers_block_buffer_);
  while (input_stream.HasMoreData()) {
    if (!DecodeNextOpcode(&input_stream))
      return false;
  }
  headers_block_buffer_.clear();

  // Emit everything in the reference set that hasn't already been emitted.
  for (size_t i = 1; i <= context_.GetMutableEntryCount(); ++i) {
    if (context_.IsReferencedAt(i) &&
        (context_.GetTouchCountAt(i) == HpackEncodingContext::kUntouched)) {
      HandleHeaderRepresentation(context_.GetNameAt(i).as_string(),
                                 context_.GetValueAt(i).as_string());
    }
    context_.ClearTouchesAt(i);
  }
  // Emit the Cookie header, if any crumbles were encountered.
  if (!cookie_name_.empty()) {
    decoded_block_[cookie_name_] = cookie_value_;
    cookie_name_.clear();
    cookie_value_.clear();
  }
  return true;
}

void HpackDecoder::HandleHeaderRepresentation(StringPiece name,
                                              StringPiece value) {
  typedef std::pair<std::map<string, string>::iterator, bool> InsertResult;

  // TODO(jgraettinger): HTTP/2 requires strict lowercasing of headers,
  // and the permissiveness here isn't wanted. Back this out in upstream.
  if (LowerCaseEqualsASCII(name.begin(), name.end(), "cookie")) {
    if (cookie_name_.empty()) {
      cookie_name_.assign(name.data(), name.size());
      cookie_value_.assign(value.data(), value.size());
    } else {
      cookie_value_ += "; ";
      cookie_value_.insert(cookie_value_.end(), value.begin(), value.end());
    }
  } else {
    InsertResult result = decoded_block_.insert(
        std::make_pair(name.as_string(), value.as_string()));
    if (!result.second) {
      result.first->second.push_back('\0');
      result.first->second.insert(result.first->second.end(),
                                  value.begin(),
                                  value.end());
    }
  }
}

bool HpackDecoder::DecodeNextOpcode(HpackInputStream* input_stream) {
  // Implements 4.4: Encoding context update. Context updates are a special-case
  // of indexed header, and must be tested prior to |kIndexedOpcode| below.
  if (input_stream->MatchPrefixAndConsume(kEncodingContextOpcode)) {
    return DecodeNextContextUpdate(input_stream);
  }
  // Implements 4.2: Indexed Header Field Representation.
  if (input_stream->MatchPrefixAndConsume(kIndexedOpcode)) {
    return DecodeNextIndexedHeader(input_stream);
  }
  // Implements 4.3.1: Literal Header Field without Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralNoIndexOpcode)) {
    return DecodeNextLiteralHeader(input_stream, false);
  }
  // Implements 4.3.2: Literal Header Field with Incremental Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralIncrementalIndexOpcode)) {
    return DecodeNextLiteralHeader(input_stream, true);
  }
  // Unrecognized opcode.
  return false;
}

bool HpackDecoder::DecodeNextContextUpdate(HpackInputStream* input_stream) {
  if (input_stream->MatchPrefixAndConsume(kEncodingContextEmptyReferenceSet)) {
    return context_.ProcessContextUpdateEmptyReferenceSet();
  }
  if (input_stream->MatchPrefixAndConsume(kEncodingContextNewMaximumSize)) {
    uint32 size = 0;
    if (!input_stream->DecodeNextUint32(&size)) {
      return false;
    }
    return context_.ProcessContextUpdateNewMaximumSize(size);
  }
  // Unrecognized encoding context update.
  return false;
}

bool HpackDecoder::DecodeNextIndexedHeader(HpackInputStream* input_stream) {
  uint32 index = 0;
  if (!input_stream->DecodeNextUint32(&index))
    return false;

  // If index == 0, |kEncodingContextOpcode| would have matched.
  CHECK_NE(index, 0u);

  if (index > context_.GetEntryCount())
    return false;

  bool emitted = false;
  // The index will be put into the reference set.
  if (!context_.IsReferencedAt(index)) {
    HandleHeaderRepresentation(context_.GetNameAt(index).as_string(),
                               context_.GetValueAt(index).as_string());
    emitted = true;
  }

  uint32 new_index = 0;
  std::vector<uint32> removed_referenced_indices;
  if (!context_.ProcessIndexedHeader(
      index, &new_index, &removed_referenced_indices)) {
    return false;
  }
  if (emitted && new_index > 0)
    context_.AddTouchesAt(new_index, 0);

  return true;
}

bool HpackDecoder::DecodeNextLiteralHeader(HpackInputStream* input_stream,
                                           bool should_index) {
  StringPiece name;
  if (!DecodeNextName(input_stream, &name))
    return false;

  StringPiece value;
  if (!DecodeNextStringLiteral(input_stream, false, &value))
    return false;

  HandleHeaderRepresentation(name, value);

  if (!should_index)
    return true;

  uint32 new_index = 0;
  std::vector<uint32> removed_referenced_indices;
  if (!context_.ProcessLiteralHeaderWithIncrementalIndexing(
      name, value, &new_index, &removed_referenced_indices)) {
    return false;
  }

  if (new_index > 0)
    context_.AddTouchesAt(new_index, 0);

  return true;
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
