// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_frame_adapter.h"

#include "base/logging.h"

namespace content {
namespace {
int WebRtcToMediaPlaneType(webrtc::PlaneType type) {
  switch (type) {
    case webrtc::kYPlane:
      return media::VideoFrame::kYPlane;
    case webrtc::kUPlane:
      return media::VideoFrame::kUPlane;
    case webrtc::kVPlane:
      return media::VideoFrame::kVPlane;
    default:
      NOTREACHED();
      return media::VideoFrame::kMaxPlanes;
  }
}
}  // anonymous namespace

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    const scoped_refptr<media::VideoFrame>& frame)
    : frame_(frame) {
}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {
}

int WebRtcVideoFrameAdapter::width() const {
  return frame_->visible_rect().width();
}

int WebRtcVideoFrameAdapter::height() const {
  return frame_->visible_rect().height();
}

const uint8_t* WebRtcVideoFrameAdapter::data(webrtc::PlaneType type) const {
  return frame_->visible_data(WebRtcToMediaPlaneType(type));
}

int WebRtcVideoFrameAdapter::stride(webrtc::PlaneType type) const {
  return frame_->stride(WebRtcToMediaPlaneType(type));
}

void* WebRtcVideoFrameAdapter::native_handle() const {
  if (frame_->HasTextures() ||
      frame_->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFERS ||
      frame_->storage_type() == media::VideoFrame::STORAGE_SHMEM)
    return frame_.get();
  return nullptr;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::NativeToI420Buffer() {
  CHECK(media::VideoFrame::IsValidConfig(
      frame_->format(), frame_->storage_type(), frame_->coded_size(),
      frame_->visible_rect(), frame_->natural_size()));
  CHECK_EQ(media::PIXEL_FORMAT_I420, frame_->format());
  CHECK(reinterpret_cast<void*>(frame_->data(media::VideoFrame::kYPlane)));
  CHECK(reinterpret_cast<void*>(frame_->data(media::VideoFrame::kUPlane)));
  CHECK(reinterpret_cast<void*>(frame_->data(media::VideoFrame::kVPlane)));
  CHECK(frame_->stride(media::VideoFrame::kYPlane));
  CHECK(frame_->stride(media::VideoFrame::kUPlane));
  CHECK(frame_->stride(media::VideoFrame::kVPlane));
  return this;
}

}  // namespace content
