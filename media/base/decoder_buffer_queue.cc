// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_queue.h"

#include "base/logging.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"

namespace media {

DecoderBufferQueue::DecoderBufferQueue()
    : earliest_valid_timestamp_(kNoTimestamp()) {
}

DecoderBufferQueue::~DecoderBufferQueue() {}

void DecoderBufferQueue::Push(const scoped_refptr<DecoderBuffer>& buffer) {
  CHECK(!buffer->end_of_stream());

  queue_.push_back(buffer);

  // TODO(scherkus): FFmpeg returns some packets with no timestamp after
  // seeking. Fix and turn this into CHECK(). See http://crbug.com/162192
  if (buffer->timestamp() == kNoTimestamp()) {
    DVLOG(1) << "Buffer has no timestamp";
    return;
  }

  if (earliest_valid_timestamp_ == kNoTimestamp()) {
    earliest_valid_timestamp_ = buffer->timestamp();
  }

  if (buffer->timestamp() < earliest_valid_timestamp_) {
    DVLOG(1)
        << "Out of order timestamps: "
        << buffer->timestamp().InMicroseconds()
        << " vs. "
        << earliest_valid_timestamp_.InMicroseconds();
    return;
  }

  earliest_valid_timestamp_ = buffer->timestamp();
  in_order_queue_.push_back(buffer);
}

scoped_refptr<DecoderBuffer> DecoderBufferQueue::Pop() {
  scoped_refptr<DecoderBuffer> buffer = queue_.front();
  queue_.pop_front();

  if (!in_order_queue_.empty() &&
      in_order_queue_.front().get() == buffer.get()) {
    in_order_queue_.pop_front();
  }

  return buffer;
}

void DecoderBufferQueue::Clear() {
  queue_.clear();
  in_order_queue_.clear();
  earliest_valid_timestamp_ = kNoTimestamp();
}

bool DecoderBufferQueue::IsEmpty() {
  return queue_.empty();
}

base::TimeDelta DecoderBufferQueue::Duration() {
  if (in_order_queue_.size() < 2)
    return base::TimeDelta();

  base::TimeDelta start = in_order_queue_.front()->timestamp();
  base::TimeDelta end = in_order_queue_.back()->timestamp();
  return end - start;
}

}  // namespace media
