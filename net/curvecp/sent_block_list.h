// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_SENT_BLOCK_LIST_H_
#define NET_CURVECP_SENT_BLOCK_LIST_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time.h"

namespace net {

class IOBufferWithSize;

// A SentBlockList manages everything needed to track a set of of blocks
// which have been sent.  It can assign new ids to new blocks, find pending
// blocks by id, or discard blocks after they have been acknowledged by the
// remote.
class SentBlockList {
 public:
  SentBlockList();
  ~SentBlockList();

  // Creates a new block to be tracked.
  // On success, returns the unique position of this buffer within the stream,
  // which can be used to lookup this block later.
  // Returns -1 if the blocklist is already full.
  int64 CreateBlock(IOBufferWithSize* buffer);

  // Tracks that a block has been sent.
  // |position| is the identifier of the block being sent.
  // |message_id| is the id of the message sent containing the block.
  // Returns false if no block for |position| exists.
  bool MarkBlockSent(int64 position, int64 message_id);

  // Acknowledges ranges from |begin_range| to |end_range|.
  // Partial acknowledgements (e.g. where the ack range spans a partial
  // block) are not supported.
  // Removes blocks covered by these ranges from tracking.
  void AcknowledgeBlocks(int64 begin_range, int64 end_range);

  // Returns a new, unique message id.
  int64 GetNewMessageId();

  // Find a block based on its |position|.
  // Returns the block, if found, or NULL otherwise.
  IOBufferWithSize* FindBlockByPosition(int64 position) const;

  // Find the last send time of a block, based the id of the last
  // message to send it.
  base::TimeTicks FindLastSendTime(int64 last_message_id) const;

  // Returns the position of the oldest sent block.
  int64 FindPositionOfOldestSentBlock() const;

  // Returns the number of blocks in the list.
  size_t size() const { return list_.size(); }

  // Returns true if the list is full and cannot take new blocks.
  bool is_full() const;

 private:
  // An UnackedBlock is a block of application data which has been sent in a
  // message, but not acknowledged by the other side yet.  We track this
  // because we may need to retransmit.
  typedef struct {
    int64 position;                  // Offset of this block in the stream.
    int64 length;                    // Number of bytes in this block.
    int64 transmissions;             // Count of transmissions of this block.
    int64 last_message_id;           // ID of last message sending this block.
    base::TimeTicks last_sent_time;  // Time of last message sending this block.
    scoped_refptr<IOBufferWithSize> data;
  } Block;

  typedef std::vector<Block> BlockList;

  // Finds a block by position and returns it index within |list_|.
  int FindBlock(int64 position) const;

  // Debugging: Writes the entire blocklist to the log.
  void LogBlockList() const;

  int64 current_message_id_;    // The current message id.
  int64 send_sequence_number_;  // The sending sequence number.
  BlockList list_;

  DISALLOW_COPY_AND_ASSIGN(SentBlockList);
};

}  // namespace net

#endif  // NET_CURVECP_SENT_BLOCK_LIST_H_
