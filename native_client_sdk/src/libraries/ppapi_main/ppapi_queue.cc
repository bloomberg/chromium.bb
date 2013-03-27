// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi_main/ppapi_queue.h"

PPAPIQueue::PPAPIQueue()
  : read_(0),
    write_(0),
    freed_(0),
    size_(0),
    array_(NULL) { }

PPAPIQueue::~PPAPIQueue() {
  // Messages may be leaked if the queue is not empty.
  assert(read_ == write_);

  delete[] array_;
}

bool PPAPIQueue::SetSize(uint32_t queue_size) {
  assert(queue_size > 0);

  if (array_) return false;

  array_ = new void*[queue_size];
  size_ = queue_size;

  memset(array_, 0, sizeof(void*) * queue_size);
  return true;
}

bool PPAPIQueue::AddNewMessage(void* msg) {
  // Writing a NULL message is illegal
  assert(array_ != NULL);
  assert(msg != NULL);

  // If the slot not empty, the queue must be full.  Calling RemoveStaleMessage
  // may create space by freeing messages that have already been read.
  if (array_[write_] != NULL) return false;

  // Write to the spot
  array_[write_] = msg;

  // Fence to make sure the payload and payload pointer are visible.
  // Since Win32 is x86 which provides ordered writes, we don't need to
  // synchronize in that case.
#ifndef WIN32
  __sync_synchronize();
#endif

  // Increment the write pointer, to signal it's readable.
  write_ = (write_ + 1) % size_;
  return true;
}

void* PPAPIQueue::RemoveStaleMessage() {
  assert(array_ != NULL);

  // If freed and read pointer are equal, this hasn't been read yet
  if (freed_ == read_) return NULL;

  assert(array_[freed_] != NULL);

  void* ret = array_[freed_];
  array_[freed_] = NULL;

  freed_ = (freed_ + 1) % size_;
  return ret;
}

void* PPAPIQueue::AcquireTopMessage() {
  // Assert that we aren't already reading a message.
  assert(last_msg_ == NULL);

  // If read and write pointers are equal, the queue is empty.
  if (read_ == write_) return NULL;

  // Track the last message to look for illegal API use.
  last_msg_ = array_[read_];
  return last_msg_;
}

void PPAPIQueue::ReleaseTopMessage(void* msg) {
  // Verify we currently acquire message.
  assert(msg != NULL);
  assert(msg == last_msg_);

  last_msg_ = NULL;

  // Signal that the message can be freed.
  read_ = (read_ + 1) % size_;
}
