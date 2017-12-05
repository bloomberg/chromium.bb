// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureBufferPool;
using media::VideoCaptureBufferPoolImpl;
using media::VideoFrame;
using media::VideoPixelFormat;

namespace viz {

InterprocessFramePool::InterprocessFramePool(int capacity)
    : buffer_pool_(new VideoCaptureBufferPoolImpl(
          std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(),
          capacity)),
      weak_factory_(this) {}

InterprocessFramePool::~InterprocessFramePool() = default;

scoped_refptr<VideoFrame> InterprocessFramePool::ReserveVideoFrame(
    const gfx::Size& size,
    VideoPixelFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BufferId ignored;  // Not applicable to this buffer pool use case.
  const BufferId buffer_id = buffer_pool_->ReserveForProducer(
      size, format, media::VideoPixelStorage::CPU,
      0 /* unused: frame_feedback_id */, &ignored);
  if (buffer_id == VideoCaptureBufferPool::kInvalidId) {
    return nullptr;
  }
  resurrectable_buffer_id_ = VideoCaptureBufferPool::kInvalidId;
  return WrapBuffer(buffer_id, size, format);
}

scoped_refptr<VideoFrame> InterprocessFramePool::ResurrectLastVideoFrame(
    const gfx::Size& expected_size,
    VideoPixelFormat expected_format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BufferId buffer_id = buffer_pool_->ResurrectLastForProducer(
      expected_size, expected_format, media::VideoPixelStorage::CPU);
  if (buffer_id != resurrectable_buffer_id_ ||
      buffer_id == VideoCaptureBufferPool::kInvalidId) {
    return nullptr;
  }
  return WrapBuffer(buffer_id, expected_size, expected_format);
}

base::OnceClosure InterprocessFramePool::HoldFrameForDelivery(
    const VideoFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = buffer_map_.find(frame);
  DCHECK(it != buffer_map_.end());
  const BufferId buffer_id = it->second;
  resurrectable_buffer_id_ = buffer_id;
  buffer_pool_->HoldForConsumers(buffer_id, 1);
  return base::BindOnce(&VideoCaptureBufferPoolImpl::RelinquishConsumerHold,
                        buffer_pool_, buffer_id, 1);
}

float InterprocessFramePool::GetUtilization() const {
  // Note: Technically, the |buffer_pool_| is completely thread-safe; so no
  // sequence check is needed here.
  return buffer_pool_->GetBufferPoolUtilization();
}

scoped_refptr<VideoFrame> InterprocessFramePool::WrapBuffer(
    BufferId buffer_id,
    const gfx::Size& size,
    VideoPixelFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Mojo takes ownership of shared memory handles. Here, the pool has created
  // and owns the original shared memory handle, but will duplicate it for Mojo.
  mojo::ScopedSharedBufferHandle buffer =
      buffer_pool_->GetHandleForInterProcessTransit(buffer_id,
                                                    false /* read-write */);
  if (!buffer.is_valid()) {
    return nullptr;
  }

  const gfx::Size& y_plane_size =
      VideoFrame::PlaneSize(format, VideoFrame::kYPlane, size);
  const int y_plane_bytes = y_plane_size.GetArea();
  const gfx::Size& u_plane_size =
      VideoFrame::PlaneSize(format, VideoFrame::kUPlane, size);
  const int u_plane_bytes = u_plane_size.GetArea();
  const gfx::Size& v_plane_size =
      VideoFrame::PlaneSize(format, VideoFrame::kVPlane, size);
  const int v_plane_bytes = v_plane_size.GetArea();
  // TODO(miu): This could all be made much simpler if we had our own ctor in
  // MojoSharedBufferVideoFrame.
  const scoped_refptr<VideoFrame> frame =
      media::MojoSharedBufferVideoFrame::Create(
          format, size, gfx::Rect(size), size, std::move(buffer),
          y_plane_bytes + u_plane_bytes + v_plane_bytes, 0 /* y_offset */,
          y_plane_bytes /* u_offset */,
          y_plane_bytes + u_plane_bytes /* v_offset */, y_plane_size.width(),
          u_plane_size.width(), v_plane_size.width(), base::TimeDelta());
  DCHECK(frame);

  // Add an entry so that HoldFrameForDelivery() can identify the buffer backing
  // this VideoFrame.
  buffer_map_[frame.get()] = buffer_id;

  // Add a destruction observer to update |buffer_map_|. A WeakPtr is used for
  // safety, just in case the VideoFrame's scope extends beyond that of this
  // InterprocessFramePool.
  frame->AddDestructionObserver(base::BindOnce(
      &InterprocessFramePool::OnFrameWrapperDestroyed,
      weak_factory_.GetWeakPtr(), base::Unretained(frame.get())));

  // The second destruction observer is a callback to the
  // VideoCaptureBufferPoolImpl directly. Since |buffer_pool_| is ref-counted,
  // this means InterprocessFramePool can be safely destroyed even if there are
  // still outstanding VideoFrames.
  frame->AddDestructionObserver(
      base::BindOnce(&VideoCaptureBufferPoolImpl::RelinquishProducerReservation,
                     buffer_pool_, buffer_id));
  return frame;
}

void InterprocessFramePool::OnFrameWrapperDestroyed(const VideoFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = buffer_map_.find(frame);
  DCHECK(it != buffer_map_.end());
  buffer_map_.erase(it);
}

}  // namespace viz
