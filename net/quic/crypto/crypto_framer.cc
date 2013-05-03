// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_framer.h"

#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_data_writer.h"

using base::StringPiece;
using std::make_pair;
using std::pair;
using std::vector;

namespace net {

namespace {

const size_t kCryptoTagSize = sizeof(uint32);
const size_t kCryptoEndOffsetSize = sizeof(uint32);
const size_t kNumEntriesSize = sizeof(uint16);
const size_t kValueLenSize = sizeof(uint16);

// OneShotVisitor is a framer visitor that records a single handshake message.
class OneShotVisitor : public CryptoFramerVisitorInterface {
 public:
  explicit OneShotVisitor(CryptoHandshakeMessage* out)
      : out_(out),
        error_(false) {
  }

  virtual void OnError(CryptoFramer* framer) OVERRIDE {
    error_ = true;
  }

  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE {
    *out_ = message;
  }

  bool error() const {
    return error_;
  }

 private:
  CryptoHandshakeMessage* const out_;
  bool error_;
};

}  // namespace

CryptoFramer::CryptoFramer()
    : visitor_(NULL),
      num_entries_(0),
      values_len_(0) {
  Clear();
}

CryptoFramer::~CryptoFramer() {}

// static
CryptoHandshakeMessage* CryptoFramer::ParseMessage(StringPiece in) {
  scoped_ptr<CryptoHandshakeMessage> msg(new CryptoHandshakeMessage);
  OneShotVisitor visitor(msg.get());
  CryptoFramer framer;

  framer.set_visitor(&visitor);
  if (!framer.ProcessInput(in) ||
      visitor.error() ||
      framer.InputBytesRemaining()) {
    return NULL;
  }

  return msg.release();
}

bool CryptoFramer::ProcessInput(StringPiece input) {
  DCHECK_EQ(QUIC_NO_ERROR, error_);
  if (error_ != QUIC_NO_ERROR) {
    return false;
  }
  // Add this data to the buffer.
  buffer_.append(input.data(), input.length());
  QuicDataReader reader(buffer_.data(), buffer_.length());

  switch (state_) {
    case STATE_READING_TAG:
      if (reader.BytesRemaining() < kCryptoTagSize) {
        break;
      }
      CryptoTag message_tag;
      reader.ReadUInt32(&message_tag);
      message_.set_tag(message_tag);
      state_ = STATE_READING_NUM_ENTRIES;
    case STATE_READING_NUM_ENTRIES:
      if (reader.BytesRemaining() < kNumEntriesSize + sizeof(uint16)) {
        break;
      }
      reader.ReadUInt16(&num_entries_);
      if (num_entries_ > kMaxEntries) {
        error_ = QUIC_CRYPTO_TOO_MANY_ENTRIES;
        return false;
      }
      uint16 padding;
      reader.ReadUInt16(&padding);

      tags_and_lengths_.reserve(num_entries_);
      state_ = STATE_READING_TAGS_AND_LENGTHS;
      values_len_ = 0;
    case STATE_READING_TAGS_AND_LENGTHS: {
      if (reader.BytesRemaining() < num_entries_ * (kCryptoTagSize +
                                                    kCryptoEndOffsetSize)) {
        break;
      }

      uint32 last_end_offset = 0;
      for (unsigned i = 0; i < num_entries_; ++i) {
        CryptoTag tag;
        reader.ReadUInt32(&tag);
        if (i > 0 && tag <= tags_and_lengths_[i-1].first) {
          if (tag == tags_and_lengths_[i-1].first) {
            error_ = QUIC_CRYPTO_DUPLICATE_TAG;
          } else {
            error_ = QUIC_CRYPTO_TAGS_OUT_OF_ORDER;
          }
          return false;
        }

        uint32 end_offset;
        reader.ReadUInt32(&end_offset);

        if (end_offset < last_end_offset) {
          error_ = QUIC_CRYPTO_TAGS_OUT_OF_ORDER;
          return false;
        }
        tags_and_lengths_.push_back(
            make_pair(tag, static_cast<size_t>(end_offset - last_end_offset)));
        last_end_offset = end_offset;
      }
      values_len_ = last_end_offset;
      state_ = STATE_READING_VALUES;
    }
    case STATE_READING_VALUES:
      if (reader.BytesRemaining() < values_len_) {
        break;
      }
      for (vector<pair<CryptoTag, size_t> >::const_iterator
           it = tags_and_lengths_.begin(); it != tags_and_lengths_.end();
           it++) {
        StringPiece value;
        reader.ReadStringPiece(&value, it->second);
        message_.SetStringPiece(it->first, value);
      }
      visitor_->OnHandshakeMessage(message_);
      Clear();
      state_ = STATE_READING_TAG;
      break;
  }
  // Save any remaining data.
  buffer_ = reader.PeekRemainingPayload().as_string();
  return true;
}

// static
QuicData* CryptoFramer::ConstructHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  if (message.tag_value_map().size() > kMaxEntries) {
    return NULL;
  }
  size_t len = kCryptoTagSize;  // message tag
  len += sizeof(uint16);  // number of map entries
  len += sizeof(uint16);  // padding.
  CryptoTagValueMap::const_iterator it = message.tag_value_map().begin();
  while (it != message.tag_value_map().end()) {
    len += kCryptoTagSize;  // tag
    len += kCryptoEndOffsetSize;  // end offset
    len += it->second.length(); // value
    ++it;
  }

  QuicDataWriter writer(len);
  if (!writer.WriteUInt32(message.tag())) {
    DCHECK(false) << "Failed to write message tag.";
    return NULL;
  }
  if (!writer.WriteUInt16(message.tag_value_map().size())) {
    DCHECK(false) << "Failed to write size.";
    return NULL;
  }
  if (!writer.WriteUInt16(0)) {
    DCHECK(false) << "Failed to write padding.";
    return NULL;
  }

  uint32 end_offset = 0;
  // Tags and offsets
  for (it = message.tag_value_map().begin();
       it != message.tag_value_map().end(); ++it) {
    if (!writer.WriteUInt32(it->first)) {
      DCHECK(false) << "Failed to write tag.";
      return NULL;
    }
    end_offset += it->second.length();
    if (!writer.WriteUInt32(end_offset)) {
      DCHECK(false) << "Failed to write end offset.";
      return NULL;
    }
  }

  // Values
  for (it = message.tag_value_map().begin();
       it != message.tag_value_map().end(); ++it) {
    if (!writer.WriteBytes(it->second.data(), it->second.length())) {
      DCHECK(false) << "Failed to write value.";
      return NULL;
    }
  }
  return new QuicData(writer.take(), len, true);
}

void CryptoFramer::Clear() {
  message_.Clear();
  tags_and_lengths_.clear();
  error_ = QUIC_NO_ERROR;
  state_ = STATE_READING_TAG;
}

}  // namespace net
