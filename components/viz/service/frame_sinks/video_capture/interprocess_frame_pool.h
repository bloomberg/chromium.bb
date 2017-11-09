// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_types.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"

namespace gfx {
class Size;
}

namespace media {
class VideoFrame;
}

namespace viz {

// A pool of VideoFrames backed by shared memory that can be transported
// efficiently across mojo service boundaries.
class VIZ_SERVICE_EXPORT InterprocessFramePool {
 public:
  explicit InterprocessFramePool(int capacity);
  ~InterprocessFramePool();

  // Reserves a buffer from the pool and creates a I420 VideoFrame to wrap its
  // shared memory. When the ref-count of the returned VideoFrame goes to zero,
  // the reservation is released. Returns null if the pool is fully utilized.
  scoped_refptr<media::VideoFrame> ReserveVideoFrame(
      const gfx::Size& size,
      media::VideoPixelFormat format);

  // Finds the last VideoFrame delivered, and attempts to re-materialize it. If
  // the attempt fails, null is returned. This is used when the client knows the
  // content of the video frame has not changed and is trying to avoid having to
  // re-populate a new VideoFrame with the same content.
  scoped_refptr<media::VideoFrame> ResurrectLastVideoFrame(
      const gfx::Size& expected_size,
      media::VideoPixelFormat expected_format);

  // Pins the memory buffer backing the given |frame| while it is being
  // delivered to, and read by, downstream consumers. This prevents the memory
  // buffer from being reserved until after the returned closure is run. The
  // caller must guarantee the returned closure is run to prevent memory leaks.
  base::OnceClosure HoldFrameForDelivery(const media::VideoFrame* frame);

  // Returns the current pool utilization, consisting of all reserved frames
  // plus those being held for delivery.
  float GetUtilization() const;

 private:
  using BufferId = decltype(media::VideoCaptureBufferPool::kInvalidId + 0);

  // Helper to build a media::MojoSharedBufferVideoFrame instance backed by a
  // specific buffer from the |buffer_pool_|.
  scoped_refptr<media::VideoFrame> WrapBuffer(BufferId buffer_id,
                                              const gfx::Size& size,
                                              media::VideoPixelFormat format);

  // Called when a reserved/resurrected VideoFrame goes out of scope, to remove
  // the entry from |buffer_map_|.
  void OnFrameWrapperDestroyed(const media::VideoFrame* frame);

  // Maintains a pool of shared memory buffers for inter-process delivery of
  // video frame data.
  const scoped_refptr<media::VideoCaptureBufferPoolImpl> buffer_pool_;

  // The VideoFrames that are currently wrapping a specific buffer in the pool.
  base::flat_map<const media::VideoFrame*, BufferId> buffer_map_;

  // The ID of the buffer that was last delivered. This is used to determine
  // whether the previous VideoFrame was actually populated, and should be
  // resurrected by ResurrectLastVideoFrame().
  BufferId resurrectable_buffer_id_ = media::VideoCaptureBufferPool::kInvalidId;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterprocessFramePool> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterprocessFramePool);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_
