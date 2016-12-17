// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/video_decoder_alsa.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

VideoDecoderAlsa::VideoDecoderAlsa()
    : delegate_(nullptr), weak_factory_(this) {}

VideoDecoderAlsa::~VideoDecoderAlsa() {}

void VideoDecoderAlsa::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

MediaPipelineBackend::BufferStatus VideoDecoderAlsa::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(delegate_);
  DCHECK(buffer);
  if (buffer->end_of_stream()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&VideoDecoderAlsa::OnEndOfStream,
                              weak_factory_.GetWeakPtr()));
  }
  return MediaPipelineBackend::kBufferSuccess;
}

void VideoDecoderAlsa::GetStatistics(Statistics* statistics) {}

bool VideoDecoderAlsa::SetConfig(const VideoConfig& config) {
  return true;
}

void VideoDecoderAlsa::OnEndOfStream() {
  delegate_->OnEndOfStream();
}

}  // namespace media
}  // namespace chromecast
