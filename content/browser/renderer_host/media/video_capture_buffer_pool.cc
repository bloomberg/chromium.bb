// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

namespace content {

const int VideoCaptureBufferPool::kInvalidId = -1;

VideoCaptureBufferPool::VideoCaptureBufferPool(int count)
    : count_(count),
      next_buffer_id_(0) {
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
  STLDeleteValues(&buffers_);
}

base::SharedMemoryHandle VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle,
    size_t* memory_size) {
  base::AutoLock lock(lock_);

  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return base::SharedMemory::NULLHandle();
  }
  base::SharedMemoryHandle remote_handle;
  buffer->shared_memory.ShareToProcess(process_handle, &remote_handle);
  *memory_size = buffer->shared_memory.requested_size();
  return remote_handle;
}

int VideoCaptureBufferPool::ReserveForProducer(size_t size,
                                               int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(size, buffer_id_to_drop);
}

void VideoCaptureBufferPool::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(buffer->held_by_producer);
  buffer->held_by_producer = false;
}

void VideoCaptureBufferPool::HoldForConsumers(
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(buffer->held_by_producer);
  DCHECK(!buffer->consumer_hold_count);

  buffer->consumer_hold_count = num_clients;
  // Note: |held_by_producer| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this buffer, e.g. a media::VideoFrame).
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK_GE(buffer->consumer_hold_count, num_clients);

  buffer->consumer_hold_count -= num_clients;
}

int VideoCaptureBufferPool::RecognizeReservedBuffer(
    base::SharedMemoryHandle maybe_belongs_to_pool) {
  base::AutoLock lock(lock_);
  for (BufferMap::iterator it = buffers_.begin(); it != buffers_.end(); it++) {
    if (it->second->shared_memory.handle() == maybe_belongs_to_pool) {
      DCHECK(it->second->held_by_producer);
      return it->first;
    }
  }
  return kInvalidId;  // Buffer is not from our pool.
}

scoped_refptr<media::VideoFrame> VideoCaptureBufferPool::ReserveI420VideoFrame(
    const gfx::Size& size,
    int rotation,
    int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);

  size_t frame_bytes =
      media::VideoFrame::AllocationSize(media::VideoFrame::I420, size);

  int buffer_id = ReserveForProducerInternal(frame_bytes, buffer_id_to_drop);
  if (buffer_id == kInvalidId)
    return NULL;

  base::Closure disposal_handler = base::Bind(
      &VideoCaptureBufferPool::RelinquishProducerReservation,
      this,
      buffer_id);

  Buffer* buffer = GetBuffer(buffer_id);
  // Wrap the buffer in a VideoFrame container.
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::WrapExternalSharedMemory(
          media::VideoFrame::I420,
          size,
          gfx::Rect(size),
          size,
          static_cast<uint8*>(buffer->shared_memory.memory()),
          frame_bytes,
          buffer->shared_memory.handle(),
          base::TimeDelta(),
          disposal_handler);

  if (buffer->rotation != rotation) {
    // TODO(jiayl): Generalize the |rotation| mechanism.
    media::FillYUV(frame.get(), 0, 128, 128);
    buffer->rotation = rotation;
  }

  return frame;
}

VideoCaptureBufferPool::Buffer::Buffer()
    : rotation(0),
      held_by_producer(false),
      consumer_hold_count(0) {}

int VideoCaptureBufferPool::ReserveForProducerInternal(size_t size,
                                                       int* buffer_id_to_drop) {
  lock_.AssertAcquired();

  // Look for a buffer that's allocated, big enough, and not in use.
  *buffer_id_to_drop = kInvalidId;
  for (BufferMap::iterator it = buffers_.begin(); it != buffers_.end(); it++) {
    Buffer* buffer = it->second;
    if (!buffer->consumer_hold_count && !buffer->held_by_producer) {
      if (buffer->shared_memory.requested_size() >= size) {
        // Existing buffer is big enough. Reuse it.
        buffer->held_by_producer = true;
        return it->first;
      }
    }
  }

  // Look for a buffer that's not in use, that we can reallocate.
  for (BufferMap::iterator it = buffers_.begin(); it != buffers_.end(); it++) {
    Buffer* buffer = it->second;
    if (!buffer->consumer_hold_count && !buffer->held_by_producer) {
      // Existing buffer is too small. Free it so we can allocate a new one
      // after the loop.
      *buffer_id_to_drop = it->first;
      buffers_.erase(it);
      delete buffer;
      break;
    }
  }

  // If possible, grow the pool by creating a new buffer.
  if (static_cast<int>(buffers_.size()) < count_) {
    int buffer_id = next_buffer_id_++;
    scoped_ptr<Buffer> buffer(new Buffer());
    if (!buffer->shared_memory.CreateAndMapAnonymous(size))
      return kInvalidId;
    buffer->held_by_producer = true;
    buffers_[buffer_id] = buffer.release();
    return buffer_id;
  }

  // The pool is at its size limit, and all buffers are in use.
  return kInvalidId;
}

VideoCaptureBufferPool::Buffer* VideoCaptureBufferPool::GetBuffer(
    int buffer_id) {
  BufferMap::iterator it = buffers_.find(buffer_id);
  if (it == buffers_.end())
    return NULL;
  return it->second;
}

}  // namespace content

