// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/received_block_list.h"

#include "net/base/io_buffer.h"

namespace net {

ReceivedBlockList::ReceivedBlockList()
    : current_position_(0),
      bytes_received_(0) {
}

ReceivedBlockList::~ReceivedBlockList() {
}

int ReceivedBlockList::bytes_available() const {
  int bytes_available = 0;
  BlockList::const_iterator it = list_.begin();
  while (it != list_.end()) {
    if (it->position != current_position_ + bytes_available)
      break;
    bytes_available += it->data->size();
    it++;
  }
  return bytes_available;
}

void ReceivedBlockList::AddBlock(int64 position, IOBuffer* data, int length) {
  if (position < bytes_received_) {
    LOG(ERROR) << "Unnecessary retransmit of " << position;
    return;
  }

  BlockList::iterator it = list_.begin();
  while (it != list_.end()) {
    if (it->position == position) {
      // Duplicate block.  Make sure they are the same length.
      DCHECK_EQ(it->data->size(), length);
      return;
    }
    if (it->position > position) {
      // The block must not overlap with the next block.
      DCHECK_LE(position + length, it->position);
      break;
    }
    it++;
  }
  Block new_block;
  new_block.position = position;
  new_block.data = new DrainableIOBuffer(data, length);
  list_.insert(it, new_block);

  ComputeBytesReceived();
}

int ReceivedBlockList::ReadBytes(IOBuffer* buffer, int size) {
  int bytes_read = 0;

  LogBlockList();

  while (size > 0 && list_.size()) {
    BlockList::iterator it = list_.begin();
    if (it->position != current_position_)
      break;
    int bytes = std::min(size, it->data->BytesRemaining());
    memcpy(buffer->data() + bytes_read, it->data->data(), bytes);
    it->data->DidConsume(bytes);
    size -= bytes;
    bytes_read += bytes;
    if (!it->data->BytesRemaining()) {
      current_position_ += it->data->size();
      list_.erase(it);
    }
  }
  return bytes_read;
}

void ReceivedBlockList::ComputeBytesReceived() {
  bytes_received_ = current_position_ + bytes_available();
}

void ReceivedBlockList::LogBlockList() const {
  LOG(INFO) << "Received Blocks: " << list_.size();
  std::string msg;
  std::ostringstream stream(msg);
  for (size_t index = 0; index < list_.size(); ++index)
     stream << "(" << list_[index].position
            << "," << list_[index].data->size() << ")";
  LOG(INFO) << stream.str();
}


}  // namespace net
