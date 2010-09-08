// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the RingBuffer class.

#include "../client/ring_buffer.h"
#include <algorithm>
#include "../client/cmd_buffer_helper.h"

namespace gpu {

RingBuffer::RingBuffer(
    Offset base_offset, unsigned int size, CommandBufferHelper* helper)
    : helper_(helper),
      base_offset_(base_offset),
      size_(size),
      free_offset_(0),
      in_use_offset_(0) {
}

RingBuffer::~RingBuffer() {
  // Free blocks pending tokens.
  while (!blocks_.empty()) {
    FreeOldestBlock();
  }
}

void RingBuffer::FreeOldestBlock() {
  GPU_DCHECK(!blocks_.empty()) << "no free blocks";
  Block& block = blocks_.front();
  GPU_DCHECK(block.valid) << "attempt to allocate more than maximum memory";
  helper_->WaitForToken(block.token);
  in_use_offset_ += block.size;
  if (in_use_offset_ == size_) {
    in_use_offset_ = 0;
  }
  // If they match then the entire buffer is free.
  if (in_use_offset_ == free_offset_) {
    in_use_offset_ = 0;
    free_offset_ = 0;
  }
  blocks_.pop_front();
}

RingBuffer::Offset RingBuffer::Alloc(unsigned int size) {
  GPU_DCHECK_LE(size, size_) << "attempt to allocate more than maximum memory";
  GPU_DCHECK(blocks_.empty() || blocks_.back().valid)
      << "Attempt to alloc another block before freeing the previous.";
  // Similarly to malloc, an allocation of 0 allocates at least 1 byte, to
  // return different pointers every time.
  if (size == 0) size = 1;

  // Wait until there is enough room.
  while (size > GetLargestFreeSizeNoWaiting()) {
    FreeOldestBlock();
  }

  Offset offset = free_offset_;
  blocks_.push_back(Block(offset, size));
  free_offset_ += size;
  if (free_offset_ == size_) {
    free_offset_ = 0;
  }
  return offset + base_offset_;
}

void RingBuffer::FreePendingToken(RingBuffer::Offset offset,
                                  unsigned int token) {
  offset -= base_offset_;
  GPU_DCHECK(!blocks_.empty()) << "no allocations to free";
  for (Container::reverse_iterator it = blocks_.rbegin();
        it != blocks_.rend();
        ++it) {
    Block& block = *it;
    if (block.offset == offset) {
      GPU_DCHECK(!block.valid)
          << "block that corresponds to offset already freed";
      block.token = token;
      block.valid = true;
      return;
    }
  }
  GPU_NOTREACHED() << "attempt to free non-existant block";
}

unsigned int RingBuffer::GetLargestFreeSizeNoWaiting() {
  // TODO(gman): Should check what the current token is and free up to that
  //    point.
  if (free_offset_ == in_use_offset_) {
    if (blocks_.empty()) {
      // The entire buffer is free.
      GPU_DCHECK_EQ(free_offset_, 0u);
      return size_;
    } else {
      // The entire buffer is in use.
      return 0;
    }
  } else if (free_offset_ > in_use_offset_) {
    // It's free from free_offset_ to size_
    return size_ - free_offset_;
  } else {
    // It's free from free_offset_ -> in_use_offset_;
    return in_use_offset_ - free_offset_;
  }
}

}  // namespace gpu
