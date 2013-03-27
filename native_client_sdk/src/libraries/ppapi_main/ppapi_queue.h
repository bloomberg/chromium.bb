// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef PPAPI_MAIN_PPAPI_QUEUE_H
#define PPAPI_MAIN_PPAPI_QUEUE_H

#include <stdint.h>

//
// PPAPIQueue
//
// PPAPIQueue is a single producer/single consumer lockless queue.  This
// implementation is PPAPI friendly because it prevents the main thread from
// ever needing to lock.   In addition, allocation and destruction of messages
// happens on the same thread, allowing for lockless message allocation as well.
//
// Messages pass through four states in order:
//   AddNewMessage - The message is added to the queue by the producer
//   and the memory is fenced to ensure it is visible.  Once the fence
//   returns, the write pointer is incremented to signal it's available to
//   the consumer.  NOTE: NULL messages are illegal.
//
//   AcquireTopMessage - Next the message is acquired by the consumer thread.
//   At this point, the consumer is considered to be examining the payload so
//   we do not increment the read pointer yet to prevent deletion.  NOTE: it
//   is illegal to acquire the next message until the previous one is released.
//
//   ReleaseTopMessage - Now the consumer signals that it is no longer looking
//   at the message by incremented the read pointer.  The producer is free to
//   release or reuse the payload.
//
//   RemoveStaleMessage - The message is no longer visible to the consumer, so
//   it is returned to the producer to be reused or destroyed.  It's location
//   in the queue is set to NULL to signal that a new message may be added in
//   that slot.
//
class PPAPIQueue {
 public:
  PPAPIQueue();
  ~PPAPIQueue();

  //
  // Producer API
  //
  // First, the producer must set the queue size before any operation can
  // take place.  Next, before adding a message, clear space in the queue,
  // by removing stale messages.  Adding a new message will return TRUE if
  // space is available, otherwise FALSE is returned and it's up to the
  // application developer to decide what to do.
  //
  bool SetSize(uint32_t queue_size);
  bool AddNewMessage(void* msg);
  void* RemoveStaleMessage();

  //
  // Consumer API
  //
  // The reader will attempt to Acquire the top message, if one is not
  // available NULL will be returned.  Once the consumer is done with the
  // message, ReleaseTopMessage is called to signal that the payload is no
  // longer visible to the consumer and can be recycled or destroyed.
  // Since messages are freed in order, it is required that messages are
  // consumed in order.  For this reason, it is illegal to call Acquire again
  // after a non-NULL message pointer is returned, until Release is called on
  // the old message.  This means the consumer can only look at one message
  // at a time.  To look at multiple messages at once, the consumer would
  // need to make a copy and release the top message.
  //
  void* AcquireTopMessage();
  void ReleaseTopMessage(void* msg);

 private:
  uint32_t read_;
  uint32_t write_;
  uint32_t freed_;
  uint32_t size_;
  void*  last_msg_;
  void** array_;
};


#endif  // PPAPI_MAIN_PPAPI_QUEUE_H
