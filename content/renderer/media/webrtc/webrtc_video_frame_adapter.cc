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

uint8_t* WebRtcVideoFrameAdapter::data(webrtc::PlaneType type) {
  NOTREACHED();
  return nullptr;
}

int WebRtcVideoFrameAdapter::stride(webrtc::PlaneType type) const {
  return frame_->stride(WebRtcToMediaPlaneType(type));
}

void* WebRtcVideoFrameAdapter::native_handle() const {
  return frame_->HasTextures() ? frame_.get() : nullptr;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer>
WebRtcVideoFrameAdapter::NativeToI420Buffer() {
  NOTREACHED();
  return nullptr;
}

}  // namespace content
