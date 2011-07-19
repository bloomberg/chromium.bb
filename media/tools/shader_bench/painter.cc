// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/shader_bench/painter.h"

Painter::Painter()
    : frames_(NULL) {
}

Painter::~Painter() {
}

void Painter::OnPaint() {
  if (frames_ && !frames_->empty()) {
    scoped_refptr<media::VideoFrame> cur_frame = frames_->front();
    Paint(cur_frame);
    frames_->pop_front();
    frames_->push_back(cur_frame);
  }
}

void Painter::LoadFrames(
    std::deque<scoped_refptr<media::VideoFrame> >* frames) {
  frames_ = frames;
}
