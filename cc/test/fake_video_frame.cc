// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_video_frame.h"

namespace cc {

FakeVideoFrame::FakeVideoFrame(const scoped_refptr<media::VideoFrame>& frame)
    : frame_(frame) {}

FakeVideoFrame::~FakeVideoFrame() {}

WebKit::WebVideoFrame::Format FakeVideoFrame::format() const {
  return FormatInvalid;
}

unsigned FakeVideoFrame::width() const {
  return frame_->natural_size().width();
}

unsigned FakeVideoFrame::height() const {
  return frame_->natural_size().height();
}

unsigned FakeVideoFrame::planes() const {
  return 0;
}

int FakeVideoFrame::stride(unsigned plane) const {
  return 0;
}

const void* FakeVideoFrame::data(unsigned plane) const {
  return NULL;
}

unsigned FakeVideoFrame::textureId() const {
  return frame_->texture_id();
}

unsigned FakeVideoFrame::textureTarget() const {
  return frame_->texture_target();
}

WebKit::WebRect FakeVideoFrame::visibleRect() const {
  return frame_->visible_rect();
}

WebKit::WebSize FakeVideoFrame::textureSize() const {
  return frame_->coded_size();
}

// static
media::VideoFrame* FakeVideoFrame::ToVideoFrame(
    WebKit::WebVideoFrame* web_video_frame) {
  if (!web_video_frame)
    return NULL;
  FakeVideoFrame* fake_frame = static_cast<FakeVideoFrame*>(web_video_frame);
  return fake_frame->frame_.get();
}

}  // namespace cc
