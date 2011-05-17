// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/sent_block_list.h"

#include "base/logging.h"
#include "net/base/io_buffer.h"

namespace net {

static const size_t kMaxBlocks = 128;

SentBlockList::SentBlockList()
    : current_message_id_(1),
      send_sequence_number_(0) {
}

SentBlockList::~SentBlockList() {
}

int64 SentBlockList::CreateBlock(IOBufferWithSize* buffer) {
  DCHECK_LT(0, buffer->size());

  if (is_full())
    return -1;

  Block new_block;
  new_block.position = send_sequence_number_;
  new_block.length = buffer->size();
  new_block.transmissions = 0;
  new_block.last_message_id = 0;
  new_block.data= buffer;

  send_sequence_number_ += buffer->size();
  list_.push_back(new_block);
  return new_block.position;
}

bool SentBlockList::MarkBlockSent(int64 position, int64 message_id) {
  int index = FindBlock(position);
  if (index < 0) {
    NOTREACHED();
    return false;
  }
  list_[index].last_message_id = message_id;
  list_[index].transmissions++;
  list_[index].last_sent_time = base::TimeTicks::Now();
  return true;
}

void SentBlockList::AcknowledgeBlocks(int64 begin_range, int64 end_range) {
  if (begin_range == end_range)
    return;

  // TODO(mbelshe): use a better data structure.
  LOG(ERROR) << "ACK of: " << begin_range << " to " << end_range;

  BlockList::iterator it = list_.begin();
  while (it != list_.end()) {
    int64 position = it->position;
    int64 length = it->length;
    if (position >= begin_range && (position + length) <= end_range) {
      list_.erase(it);
      it = list_.begin();  // iterator was invalidated, so go to start of list.
      continue;
    }

    // Verify we didn't have a partial block acknowledgement.
    CHECK(position < begin_range || position >= end_range);
    CHECK((position + length) < begin_range || (position + length) > end_range);
    it++;
  }
}

int64 SentBlockList::GetNewMessageId() {
  return current_message_id_++;
}

IOBufferWithSize* SentBlockList::FindBlockByPosition(int64 position) const {
  int index = FindBlock(position);
  if (index < 0)
    return NULL;
  return list_[index].data;
}

base::TimeTicks SentBlockList::FindLastSendTime(int64 last_message_id) const {
  for (size_t index = 0; index < list_.size(); ++index)
    if (list_[index].last_message_id == last_message_id)
      return list_[index].last_sent_time;
  return base::TimeTicks();
}

int SentBlockList::FindBlock(int64 position) const {
  for (size_t index = 0; index < list_.size(); ++index)
    if (list_[index].position == position)
      return index;
  return -1;
}

int64 SentBlockList::FindPositionOfOldestSentBlock() const {
  base::TimeTicks oldest_time;
  int64 position = -1;

  LogBlockList();

  // Walks the entire list.
  for (size_t index = 0; index < list_.size(); ++index) {
    base::TimeTicks last_sent_time = list_[index].last_sent_time;
    if (!last_sent_time.is_null()) {
      if (last_sent_time < oldest_time || oldest_time.is_null()) {
        oldest_time = last_sent_time;
        position = list_[index].position;
      }
    }
  }
  return position;
}
bool SentBlockList::is_full() const {
  return list_.size() == kMaxBlocks;
}

void SentBlockList::LogBlockList() const {
  LOG(INFO) << "Sent Blocks: " << list_.size();
  std::string msg;
  std::ostringstream stream(msg);
  for (size_t index = 0; index < list_.size(); ++index)
     stream << "(" << list_[index].position
            << "," << list_[index].length << ")";
  LOG(INFO) << stream.str();
}

}  // namespace net
