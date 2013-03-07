// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "ui/gfx/size.h"

namespace content {

// A thread-safe class that does the bookkeeping and lifetime management for a
// pool of shared-memory pixel buffers cycled between an in-process producer
// (e.g. a VideoCaptureDevice) and a set of out-of-process consumers. The pool
// is intended to be allocated and orchestrated by a VideoCaptureController, but
// is designed to outlive the controller if necessary.
//
// Producers access buffers by means of a VideoFrame container. Producers get
// a buffer by calling ReserveForProducer(), and pass on their ownership
// of the buffer by calling HoldForConsumers(). Consumers signal that they
// are done with the buffer by calling RelinquishConsumerHold().
//
// Buffers are identified by an int value called |buffer_id|. Callers may depend
// on the buffer IDs being dense in the range 1 to count(), inclusive, so long
// as the Allocate() step succeeded. 0 is never a valid ID, and is returned by
// some methods to indicate failure.
class CONTENT_EXPORT VideoCaptureBufferPool
    : public base::RefCountedThreadSafe<VideoCaptureBufferPool> {
 public:
  VideoCaptureBufferPool(const gfx::Size& size, int count);

  // One-time initialization to allocate the shared memory buffers. Returns true
  // on success.
  bool Allocate();

  // One-time (per client/per-buffer) initialization to share a particular
  // buffer to a process.
  base::SharedMemoryHandle ShareToProcess(int buffer_id,
                                          base::ProcessHandle process_handle);

  // Locate a buffer (if any) that's not in use by the producer or consumers,
  // and reserve it as a VideoFrame. The buffer remains reserved (and writable
  // by the producer) until either the VideoFrame is destroyed, or until
  // ownership is transferred to the consumer via HoldForConsumers().
  //
  // The resulting VideoFrames will reference this VideoCaptureBufferPool, and
  // so the pool will stay alive so long as these VideoFrames do.
  //
  // |rotation| is used for a clear optimization.
  scoped_refptr<media::VideoFrame> ReserveForProducer(int rotation);

  // Transfer a buffer from producer to consumer ownership.
  // |producer_held_buffer| must be a frame previously returned by
  // ReserveForProducer(), and not already passed to HoldForConsumers(). The
  // caller should promptly release all references to |producer_held_buffer|
  // after calling this method, so that the buffer may be eventually reused once
  // the consumers finish with it.
  void HoldForConsumers(
      const scoped_refptr<media::VideoFrame>& producer_held_buffer,
      int buffer_id,
      int num_clients);

  // Indicate that one or more consumers are done with a particular buffer. This
  // effectively is the opposite of HoldForConsumers(). Once the consumers are
  // done, a buffer is returned to the pool for reuse.
  void RelinquishConsumerHold(int buffer_id, int num_clients);

  // Detect whether a particular VideoFrame is backed by a buffer that belongs
  // to this pool -- that is, whether it was allocated by an earlier call to
  // ReserveForProducer(). If so, return its buffer_id (a value between 1 and
  // count()). If not, return 0, indicating the buffer is not recognized (it may
  // be a valid frame, but we didn't allocate it).
  int RecognizeReservedBuffer(
      const scoped_refptr<media::VideoFrame>& maybe_belongs_to_pool);

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

  // Callback for VideoFrame destruction.
  void OnVideoFrameDestroyed(int buffer_id);

  bool IsAllocated() const;

  // Protects |buffers_| and contents thereof.
  base::Lock lock_;

  // The buffers, indexed by |buffer_id|. Element 0 is always NULL.
  ScopedVector<Buffer> buffers_;

  const gfx::Size size_;
  const int count_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureBufferPool);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
