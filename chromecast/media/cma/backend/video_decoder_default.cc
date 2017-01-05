// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_decoder_default.h"

#include "base/memory/ptr_util.h"
#include "chromecast/media/cma/backend/media_sink_default.h"

namespace chromecast {
namespace media {

VideoDecoderDefault::VideoDecoderDefault() {}

VideoDecoderDefault::~VideoDecoderDefault() {}

void VideoDecoderDefault::Start(base::TimeDelta start_pts) {
  DCHECK(!sink_);
  sink_ = base::MakeUnique<MediaSinkDefault>(delegate_, start_pts);
}

void VideoDecoderDefault::Stop() {
  DCHECK(sink_);
  sink_.reset();
}

void VideoDecoderDefault::SetPlaybackRate(float rate) {
  DCHECK(sink_);
  sink_->SetPlaybackRate(rate);
}

base::TimeDelta VideoDecoderDefault::GetCurrentPts() {
  DCHECK(sink_);
  return sink_->GetCurrentPts();
}

void VideoDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(!sink_);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(sink_);
  return sink_->PushBuffer(buffer);
}

void VideoDecoderDefault::GetStatistics(Statistics* statistics) {
}

bool VideoDecoderDefault::SetConfig(const VideoConfig& config) {
  return true;
}

}  // namespace media
}  // namespace chromecast
