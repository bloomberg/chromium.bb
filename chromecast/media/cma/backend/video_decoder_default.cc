// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_decoder_default.h"

#include "base/logging.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

VideoDecoderDefault::VideoDecoderDefault() : delegate_(nullptr) {}

VideoDecoderDefault::~VideoDecoderDefault() {}

void VideoDecoderDefault::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderDefault::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);
  if (buffer->end_of_stream())
    delegate_->OnEndOfStream();
  return MediaPipelineBackend::kBufferSuccess;
}

void VideoDecoderDefault::GetStatistics(Statistics* statistics) {
}

bool VideoDecoderDefault::SetConfig(const VideoConfig& config) {
  return true;
}

}  // namespace media
}  // namespace chromecast
