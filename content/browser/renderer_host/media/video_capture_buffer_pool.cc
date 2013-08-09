// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

namespace content {

VideoCaptureBufferPool::VideoCaptureBufferPool(size_t size, int count)
    : size_(size),
      count_(count) {
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
}

bool VideoCaptureBufferPool::Allocate() {
  base::AutoLock lock(lock_);
  DCHECK(!IsAllocated());
  buffers_.resize(count_);
  for (int buffer_id = 0; buffer_id < count(); ++buffer_id) {
    Buffer* buffer = new Buffer();
    buffers_[buffer_id] = buffer;
    if (!buffer->shared_memory.CreateAndMapAnonymous(GetMemorySize()))
      return false;
  }
  return true;
}

base::SharedMemoryHandle VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle) {
  base::AutoLock lock(lock_);
  DCHECK(IsAllocated());
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count_);
  Buffer* buffer = buffers_[buffer_id];
  base::SharedMemoryHandle remote_handle;
  buffer->shared_memory.ShareToProcess(process_handle, &remote_handle);
  return remote_handle;
}

base::SharedMemoryHandle VideoCaptureBufferPool::GetHandle(int buffer_id) {
  base::AutoLock lock(lock_);
  DCHECK(IsAllocated());
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count_);
  return buffers_[buffer_id]->shared_memory.handle();
}

void* VideoCaptureBufferPool::GetMemory(int buffer_id) {
  base::AutoLock lock(lock_);
  DCHECK(IsAllocated());
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count_);
  return buffers_[buffer_id]->shared_memory.memory();
}

int VideoCaptureBufferPool::ReserveForProducer() {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal();
}

void VideoCaptureBufferPool::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count());
  Buffer* buffer = buffers_[buffer_id];
  DCHECK(buffer->held_by_producer);
  buffer->held_by_producer = false;
}

void VideoCaptureBufferPool::HoldForConsumers(
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count());
  DCHECK(IsAllocated());
  Buffer* buffer = buffers_[buffer_id];
  DCHECK(buffer->held_by_producer);
  DCHECK(!buffer->consumer_hold_count);

  buffer->consumer_hold_count = num_clients;
  // Note: |held_by_producer| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this buffer, e.g. a media::VideoFrame
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  DCHECK(buffer_id >= 0);
  DCHECK(buffer_id < count());
  DCHECK_GT(num_clients, 0);
  DCHECK(IsAllocated());
  Buffer* buffer = buffers_[buffer_id];
  DCHECK_GE(buffer->consumer_hold_count, num_clients);

  buffer->consumer_hold_count -= num_clients;
}

// State query functions.
size_t VideoCaptureBufferPool::GetMemorySize() const {
  // No need to take |lock_| currently.
  return size_;
}

int VideoCaptureBufferPool::RecognizeReservedBuffer(
    base::SharedMemoryHandle maybe_belongs_to_pool) {
  base::AutoLock lock(lock_);
  for (int buffer_id = 0; buffer_id < count(); ++buffer_id) {
    Buffer* buffer = buffers_[buffer_id];
    if (buffer->shared_memory.handle() == maybe_belongs_to_pool) {
      DCHECK(buffer->held_by_producer);
      return buffer_id;
    }
  }
  return -1;  // Buffer is not from our pool.
}

scoped_refptr<media::VideoFrame> VideoCaptureBufferPool::ReserveI420VideoFrame(
    const gfx::Size& size,
    int rotation) {
  if (static_cast<size_t>(size.GetArea() * 3 / 2) != GetMemorySize())
    return NULL;

  base::AutoLock lock(lock_);

  int buffer_id = ReserveForProducerInternal();
  if (buffer_id < 0)
    return NULL;

  base::Closure disposal_handler = base::Bind(
      &VideoCaptureBufferPool::RelinquishProducerReservation,
      this,
      buffer_id);

  Buffer* buffer = buffers_[buffer_id];
  // Wrap the buffer in a VideoFrame container.
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalSharedMemory(
          media::VideoFrame::I420,
          size,
          gfx::Rect(size),
          size,
          static_cast<uint8*>(buffer->shared_memory.memory()),
          buffer->shared_memory.handle(),
          base::TimeDelta(),
          disposal_handler);

  if (buffer->rotation != rotation) {
    // TODO(nick): Generalize the |rotation| mechanism.
    media::FillYUV(frame.get(), 0, 128, 128);
    buffer->rotation = rotation;
  }

  return frame;
}

bool VideoCaptureBufferPool::IsAnyBufferHeldForConsumers() {
  base::AutoLock lock(lock_);
  for (int buffer_id = 0; buffer_id < count(); ++buffer_id) {
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

int VideoCaptureBufferPool::ReserveForProducerInternal() {
  lock_.AssertAcquired();
  DCHECK(IsAllocated());

  int buffer_id = -1;
  for (int candidate_id = 0; candidate_id < count(); ++candidate_id) {
    Buffer* candidate = buffers_[candidate_id];
    if (!candidate->consumer_hold_count && !candidate->held_by_producer) {
      buffer_id = candidate_id;
      break;
    }
  }
  if (buffer_id == -1)
    return -1;

  Buffer* buffer = buffers_[buffer_id];
  CHECK_GE(buffer->shared_memory.requested_size(), size_);
  buffer->held_by_producer = true;
  return buffer_id;
}

bool VideoCaptureBufferPool::IsAllocated() const {
  lock_.AssertAcquired();
  return !buffers_.empty();
}

}  // namespace content

