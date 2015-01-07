// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/video_frame_compositor.h"

#include "media/base/video_frame.h"

namespace media {

static bool IsOpaque(const scoped_refptr<VideoFrame>& frame) {
  switch (frame->format()) {
    case VideoFrame::UNKNOWN:
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV24:
    case VideoFrame::NV12:
      return true;

    case VideoFrame::YV12A:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
      break;
  }
  return false;
}

VideoFrameCompositor::VideoFrameCompositor(
    const base::Callback<void(gfx::Size)>& natural_size_changed_cb,
    const base::Callback<void(bool)>& opacity_changed_cb)
    : natural_size_changed_cb_(natural_size_changed_cb),
      opacity_changed_cb_(opacity_changed_cb),
      client_(NULL) {
}

VideoFrameCompositor::~VideoFrameCompositor() {
  if (client_)
    client_->StopUsingProvider();
}

void VideoFrameCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  if (client_)
    client_->StopUsingProvider();
  client_ = client;
}

scoped_refptr<VideoFrame> VideoFrameCompositor::GetCurrentFrame() {
  return current_frame_;
}

void VideoFrameCompositor::PutCurrentFrame(
    const scoped_refptr<VideoFrame>& frame) {
}

void VideoFrameCompositor::UpdateCurrentFrame(
    const scoped_refptr<VideoFrame>& frame) {
  if (current_frame_.get() &&
      current_frame_->natural_size() != frame->natural_size()) {
    natural_size_changed_cb_.Run(frame->natural_size());
  }

  if (!current_frame_.get() || IsOpaque(current_frame_) != IsOpaque(frame)) {
    opacity_changed_cb_.Run(IsOpaque(frame));
  }

  current_frame_ = frame;

  if (client_)
    client_->DidReceiveFrame();
}

}  // namespace media
