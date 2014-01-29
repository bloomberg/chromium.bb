// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_headers_block_parser.h"

#include <memory>

#include "base/sys_byteorder.h"

namespace net {

SpdyHeadersBlockParserReader::SpdyHeadersBlockParserReader(
    base::StringPiece prefix,
    base::StringPiece suffix)
      : prefix_(prefix),
        suffix_(suffix),
        in_suffix_(false),
        offset_(0) {}

size_t SpdyHeadersBlockParserReader::Available() {
  if (in_suffix_) {
    return suffix_.length() - offset_;
  } else {
    return prefix_.length() + suffix_.length() - offset_;
  }
}

bool SpdyHeadersBlockParserReader::ReadN(size_t count, char* out) {
  if (Available() < count)
    return false;

  if (!in_suffix_ && count > (prefix_.length() - offset_)) {
    count -= prefix_.length() - offset_;
    out = std::copy(prefix_.begin() + offset_, prefix_.end(), out);
    in_suffix_ = true;
    offset_ = 0;
    // Fallthrough to suffix read.
  } else if (!in_suffix_) {
    // Read is satisfied by the prefix.
    DCHECK_GE(prefix_.length() - offset_, count);
    std::copy(prefix_.begin() + offset_,
              prefix_.begin() + offset_ + count,
              out);
    offset_ += count;
    return true;
  }
  // Read is satisfied by the suffix.
  DCHECK(in_suffix_);
  DCHECK_GE(suffix_.length() - offset_, count);
  std::copy(suffix_.begin() + offset_,
            suffix_.begin() + offset_ + count,
            out);
  offset_ += count;
  return true;
}

std::vector<char> SpdyHeadersBlockParserReader::Remainder() {
  std::vector<char> remainder(Available(), '\0');
  if (remainder.size()) {
    ReadN(remainder.size(), &remainder[0]);
  }
  DCHECK_EQ(0u, Available());
  return remainder;
}

SpdyHeadersBlockParser::SpdyHeadersBlockParser(
    SpdyHeadersHandlerInterface* handler) : state_(READING_HEADER_BLOCK_LEN),
    remaining_key_value_pairs_for_frame_(0), next_field_len_(0),
    handler_(handler) {
}

SpdyHeadersBlockParser::~SpdyHeadersBlockParser() {}

bool SpdyHeadersBlockParser::ParseUInt32(Reader* reader,
                                         uint32_t* parsed_value) {
  // Are there enough bytes available?
  if (reader->Available() < sizeof(uint32_t)) {
    return false;
  }

  // Read the required bytes, convert from network to host
  // order and return the parsed out integer.
  char buf[sizeof(uint32_t)];
  reader->ReadN(sizeof(uint32_t), buf);
  *parsed_value = base::NetToHost32(*reinterpret_cast<const uint32_t *>(buf));
  return true;
}

void SpdyHeadersBlockParser::Reset() {
  // Clear any saved state about the last headers block.
  headers_block_prefix_.clear();
  state_ = READING_HEADER_BLOCK_LEN;
}

void SpdyHeadersBlockParser::HandleControlFrameHeadersData(
    const char* headers_data, size_t len) {
  // Only do something if we received anything new.
  if (len == 0) {
    return;
  }

  // Reader avoids copying data.
  base::StringPiece prefix;
  if (headers_block_prefix_.size()) {
    prefix.set(&headers_block_prefix_[0], headers_block_prefix_.size());
  }
  Reader reader(prefix, base::StringPiece(headers_data, len));

  // If we didn't parse out yet the number of key-value pairs in this
  // headers block, try to do it now (succeeds if we received enough bytes).
  if (state_ == READING_HEADER_BLOCK_LEN) {
    if (ParseUInt32(&reader, &remaining_key_value_pairs_for_frame_)) {
      handler_->OnHeaderBlock(remaining_key_value_pairs_for_frame_);
      state_ = READING_KEY_LEN;
    } else {
      headers_block_prefix_ = reader.Remainder();
      return;
    }
  }

  // Parse out and handle the key-value pairs.
  while (remaining_key_value_pairs_for_frame_ > 0) {
    // Parse the key-value length, in case we don't already have it.
    if ((state_ == READING_KEY_LEN) || (state_ == READING_VALUE_LEN)) {
      if (ParseUInt32(&reader, &next_field_len_)) {
        state_ == READING_KEY_LEN ? state_ = READING_KEY :
                                    state_ = READING_VALUE;
      } else {
        // Did not receive enough bytes.
        break;
      }
    }

    // Parse the next field if we received enough bytes.
    if (reader.Available() >= next_field_len_) {
      // Copy the field from the cord.
      char* key_value_buf(new char[next_field_len_]);
      reader.ReadN(next_field_len_, key_value_buf);

      // Is this field a key?
      if (state_ == READING_KEY) {
        current_key.reset(key_value_buf);
        key_len_ = next_field_len_;
        state_ = READING_VALUE_LEN;

      } else if (state_ == READING_VALUE) {
        // We already had the key, now we got its value.
        current_value.reset(key_value_buf);

        // Call the handler for the key-value pair that we received.
        handler_->OnKeyValuePair(
            base::StringPiece(current_key.get(), key_len_),
            base::StringPiece(current_value.get(), next_field_len_));

        // Free allocated key and value strings.
        current_key.reset();
        current_value.reset();

        // Finished handling a key-value pair.
        remaining_key_value_pairs_for_frame_--;

        // Finished handling a header, prepare for the next one.
        state_ = READING_KEY_LEN;
      }
    } else {
      // Did not receive enough bytes.
      break;
    }
  }

  // Unread suffix becomes the prefix upon next invocation.
  headers_block_prefix_ = reader.Remainder();

  // Did we finish handling the current block?
  if (remaining_key_value_pairs_for_frame_ == 0) {
    handler_->OnHeaderBlockEnd();
    Reset();
  }
}

}  // namespace net
