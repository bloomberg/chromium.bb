// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace media {

class VideoFrame;

}  // namespace media

namespace content {

// A thread-safe class that does the bookkeeping and lifetime management for a
// pool of shared-memory pixel buffers cycled between an in-process producer
// (e.g. a VideoCaptureDevice) and a set of out-of-process consumers. The pool
// is intended to be allocated and orchestrated by a VideoCaptureController, but
// is designed to outlive the controller if necessary.
//
// Buffers are identified by an int value called |buffer_id|. Callers may depend
// on the buffer IDs being dense in the range [0, count()), so long as the
// Allocate() step succeeded. -1 is never a valid ID, and is returned by some
// methods to indicate failure. Producers get a buffer by calling
// ReserveForProducer(), and may pass on their ownership to the consumer by
// calling HoldForConsumers(), or drop the buffer (without further processing)
// by calling ReserveForProducer(). Consumers signal that they are done with the
// buffer by calling RelinquishConsumerHold().
class CONTENT_EXPORT VideoCaptureBufferPool
    : public base::RefCountedThreadSafe<VideoCaptureBufferPool> {
 public:
  VideoCaptureBufferPool(size_t size, int count);

  // One-time initialization to allocate the shared memory buffers. Returns true
  // on success.
  bool Allocate();

  // One-time (per client/per-buffer) initialization to share a particular
  // buffer to a process.
  base::SharedMemoryHandle ShareToProcess(int buffer_id,
                                          base::ProcessHandle process_handle);

  // Get the shared memory handle for a particular buffer index.
  base::SharedMemoryHandle GetHandle(int buffer_id);

  // Get the mapped buffer memory for a particular buffer index.
  void* GetMemory(int buffer_id);

  // Locate the index of a buffer (if any) that's not in use by the producer or
  // consumers, and reserve it. The buffer remains reserved (and writable by the
  // producer) until ownership is transferred either to the consumer via
  // HoldForConsumers(), or back to the pool with
  // RelinquishProducerReservation().
  int ReserveForProducer();

  // Indicate that a buffer held for the producer should be returned back to the
  // pool without passing on to the consumer. This effectively is the opposite
  // of ReserveForProducer().
  void RelinquishProducerReservation(int buffer_id);

  // Transfer a buffer from producer to consumer ownership.
  // |buffer_id| must be a buffer index previously returned by
  // ReserveForProducer(), and not already passed to HoldForConsumers().
  void HoldForConsumers(int buffer_id, int num_clients);

  // Indicate that one or more consumers are done with a particular buffer. This
  // effectively is the opposite of HoldForConsumers(). Once the consumers are
  // done, a buffer is returned to the pool for reuse.
  void RelinquishConsumerHold(int buffer_id, int num_clients);

  // Detect whether a particular SharedMemoryHandle is exported by a buffer that
  // belongs to this pool -- that is, whether it was allocated by an earlier
  // call to ReserveForProducer(). If so, return its buffer_id (a value on the
  // range [0, count())). If not, return -1, indicating the buffer is not
  // recognized (it may be a valid frame, but we didn't allocate it).
  int RecognizeReservedBuffer(base::SharedMemoryHandle maybe_belongs_to_pool);

  // Utility functions to return a buffer wrapped in a useful type.
  scoped_refptr<media::VideoFrame> ReserveI420VideoFrame(const gfx::Size& size,
                                                         int rotation);

  int count() const { return count_; }
  size_t GetMemorySize() const;
  bool IsAnyBufferHeldForConsumers();

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureBufferPool>;

  // Per-buffer state.
  struct Buffer {
    Buffer();

    // The memory created to be shared with renderer processes.
    base::SharedMemory shared_memory;

    // Rotation in degrees of the buffer.
    int rotation;

    // Tracks whether this buffer is currently referenced by the producer.
    bool held_by_producer;

    // Number of consumer processes which hold this shared memory.
    int consumer_hold_count;
  };

  virtual ~VideoCaptureBufferPool();

  int ReserveForProducerInternal();

  bool IsAllocated() const;

  // Protects |buffers_| and contents thereof.
  base::Lock lock_;

  // The buffers, indexed by |buffer_id|. Element 0 is always NULL.
  ScopedVector<Buffer> buffers_;

  const size_t size_;
  const int count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureBufferPool);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
