// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "media/base/video_util.h"

namespace content {

VideoCaptureBufferPool::VideoCaptureBufferPool(const gfx::Size& size, int count)
    : size_(size),
      count_(count) {
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
}

bool VideoCaptureBufferPool::Allocate() {
  base::AutoLock lock(lock_);
  DCHECK(!IsAllocated());
  buffers_.resize(count_ + 1);
  buffers_[0] = NULL;
  for (int buffer_id = 1; buffer_id <= count(); ++buffer_id) {
    Buffer* buffer = new Buffer();
    buffers_[buffer_id] = buffer;
    if (!buffer->shared_memory.CreateAndMapAnonymous(GetMemorySize())) {
      return false;
    }
  }
  return true;
}

base::SharedMemoryHandle VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle) {
  base::AutoLock lock(lock_);
  DCHECK(IsAllocated());
  DCHECK(buffer_id >= 1);
  DCHECK(buffer_id <= count_);
  Buffer* buffer = buffers_[buffer_id];
  base::SharedMemoryHandle remote_handle;
  buffer->shared_memory.ShareToProcess(process_handle, &remote_handle);
  return remote_handle;
}

scoped_refptr<media::VideoFrame> VideoCaptureBufferPool::ReserveForProducer(
    int rotation) {
  base::AutoLock lock(lock_);
  DCHECK(IsAllocated());

  int buffer_id = 0;
  for (int candidate_id = 1; candidate_id <= count(); ++candidate_id) {
    Buffer* candidate = buffers_[candidate_id];
    if (!candidate->consumer_hold_count && !candidate->held_by_producer) {
      buffer_id = candidate_id;
      break;
    }
  }
  if (!buffer_id)
    return NULL;

  Buffer* buffer = buffers_[buffer_id];

  CHECK_GE(buffer->shared_memory.requested_size(), GetMemorySize());

  // Complete the reservation.
  buffer->held_by_producer = true;
  base::Closure disposal_handler = base::Bind(
      &VideoCaptureBufferPool::OnVideoFrameDestroyed, this, buffer_id);

  // Wrap the buffer in a VideoFrame container.
  uint8* base_ptr = static_cast<uint8*>(buffer->shared_memory.memory());
  size_t u_offset = size_.GetArea();
  size_t v_offset = u_offset + u_offset / 4;
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalYuvData(
          media::VideoFrame::YV12,  // Actually it's I420, but equivalent here.
          size_, gfx::Rect(size_), size_,
          size_.width(),            // y stride
          size_.width() / 2,        // u stride
          size_.width() / 2,        // v stride
          base_ptr,                 // y address
          base_ptr + u_offset,      // u address
          base_ptr + v_offset,      // v address
          base::TimeDelta(),        // timestamp (unused).
          disposal_handler);

  if (buffer->rotation != rotation) {
    // TODO(nick): Generalize the |rotation| mechanism.
    media::FillYUV(frame, 0, 128, 128);
    buffer->rotation = rotation;
  }

  return frame;
}

void VideoCaptureBufferPool::HoldForConsumers(
    const scoped_refptr<media::VideoFrame>& producer_held_buffer,
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id >= 1);
  DCHECK(buffer_id <= count());
  DCHECK(IsAllocated());
  Buffer* buffer = buffers_[buffer_id];
  DCHECK(buffer->held_by_producer);
  DCHECK(!buffer->consumer_hold_count);
  DCHECK(producer_held_buffer->data(media::VideoFrame::kYPlane) ==
         buffer->shared_memory.memory());

  buffer->consumer_hold_count = num_clients;
  // Note: |held_by_producer| should stay true until ~VideoFrame occurs.
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id >= 1);
  DCHECK(buffer_id <= count());
  DCHECK_GT(num_clients, 0);
  DCHECK(IsAllocated());
  Buffer* buffer = buffers_[buffer_id];
  DCHECK_GE(buffer->consumer_hold_count, num_clients);

  buffer->consumer_hold_count -= num_clients;
}

  // State query functions.
size_t VideoCaptureBufferPool::GetMemorySize() const {
  // No need to take |lock_| currently.
  return size_.GetArea() * 3 / 2;
}

int VideoCaptureBufferPool::RecognizeReservedBuffer(
    const scoped_refptr<media::VideoFrame>& maybe_belongs_to_pool) {
  base::AutoLock lock(lock_);
  uint8* base_ptr = maybe_belongs_to_pool->data(media::VideoFrame::kYPlane);
  for (int buffer_id = 1; buffer_id <= count(); ++buffer_id) {
    Buffer* buffer = buffers_[buffer_id];
    if (buffer->shared_memory.memory() == base_ptr) {
      DCHECK(buffer->held_by_producer);
      return buffer_id;
    }
  }
  return 0;  // VideoFrame is not from our pool.
}

bool VideoCaptureBufferPool::IsAnyBufferHeldForConsumers() {
  base::AutoLock lock(lock_);
  for (int buffer_id = 1; buffer_id <= count(); ++buffer_id) {
    Buffer* buffer = buffers_[buffer_id];
    if (buffer->consumer_hold_count > 0)
      return true;
  }
  return false;
}

VideoCaptureBufferPool::Buffer::Buffer()
    : rotation(0),
      held_by_producer(false),
      consumer_hold_count(0) {}

void VideoCaptureBufferPool::OnVideoFrameDestroyed(int buffer_id) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id);
  Buffer* buffer = buffers_[buffer_id];
  DCHECK(buffer->held_by_producer);
  buffer->held_by_producer = false;
}

bool VideoCaptureBufferPool::IsAllocated() const {
  lock_.AssertAcquired();
  return !buffers_.empty();
}

}  // namespace content

