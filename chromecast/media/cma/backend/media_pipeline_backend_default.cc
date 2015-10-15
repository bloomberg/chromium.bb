// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_default.h"

#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

class MediaPipelineBackendDefault::AudioDecoderDefault
    : public MediaPipelineBackend::AudioDecoder {
 public:
  AudioDecoderDefault() : delegate_(nullptr) {}
  ~AudioDecoderDefault() override {}

  void SetDelegate(MediaPipelineBackend::Delegate* delegate) {
    delegate_ = delegate;
  }

  // MediaPipelineBackend::AudioDecoder implementation:
  BufferStatus PushBuffer(DecryptContext* decrypt_context,
                          CastDecoderBuffer* buffer) override {
    if (buffer->end_of_stream())
      delegate_->OnEndOfStream(this);
    return MediaPipelineBackend::kBufferSuccess;
  }

  void GetStatistics(Statistics* statistics) override {}

  bool SetConfig(const AudioConfig& config) override { return true; }

  bool SetVolume(float multiplier) override { return true; }

  RenderingDelay GetRenderingDelay() override { return RenderingDelay(); }

 private:
  MediaPipelineBackend::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderDefault);
};

class MediaPipelineBackendDefault::VideoDecoderDefault
    : public MediaPipelineBackend::VideoDecoder {
 public:
  VideoDecoderDefault() : delegate_(nullptr) {}
  ~VideoDecoderDefault() override {}

  void SetDelegate(MediaPipelineBackend::Delegate* delegate) {
    delegate_ = delegate;
  }

  // MediaPipelineBackend::VideoDecoder implementation:
  BufferStatus PushBuffer(DecryptContext* decrypt_context,
                          CastDecoderBuffer* buffer) override {
    if (buffer->end_of_stream())
      delegate_->OnEndOfStream(this);
    return MediaPipelineBackend::kBufferSuccess;
  }

  void GetStatistics(Statistics* statistics) override {}

  bool SetConfig(const VideoConfig& config) override { return true; }

 private:
  MediaPipelineBackend::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderDefault);
};

MediaPipelineBackendDefault::MediaPipelineBackendDefault()
    : running_(false), rate_(1.0f) {
}

MediaPipelineBackendDefault::~MediaPipelineBackendDefault() {
}

MediaPipelineBackend::AudioDecoder*
MediaPipelineBackendDefault::CreateAudioDecoder() {
  DCHECK(!audio_decoder_);
  audio_decoder_.reset(new AudioDecoderDefault());
  return audio_decoder_.get();
}

MediaPipelineBackend::VideoDecoder*
MediaPipelineBackendDefault::CreateVideoDecoder() {
  DCHECK(!video_decoder_);
  video_decoder_.reset(new VideoDecoderDefault());
  return video_decoder_.get();
}

bool MediaPipelineBackendDefault::Initialize(Delegate* delegate) {
  DCHECK(delegate);
  if (audio_decoder_)
    audio_decoder_->SetDelegate(delegate);
  if (video_decoder_)
    video_decoder_->SetDelegate(delegate);
  return true;
}

bool MediaPipelineBackendDefault::Start(int64_t start_pts) {
  DCHECK(!running_);
  start_pts_ = base::TimeDelta::FromMicroseconds(start_pts);
  start_clock_ = base::TimeTicks::Now();
  running_ = true;
  return true;
}

bool MediaPipelineBackendDefault::Stop() {
  start_pts_ = base::TimeDelta::FromMicroseconds(GetCurrentPts());
  running_ = false;
  return true;
}

bool MediaPipelineBackendDefault::Pause() {
  DCHECK(running_);
  start_pts_ = base::TimeDelta::FromMicroseconds(GetCurrentPts());
  running_ = false;
  return true;
}

bool MediaPipelineBackendDefault::Resume() {
  DCHECK(!running_);
  running_ = true;
  start_clock_ = base::TimeTicks::Now();
  return true;
}

int64_t MediaPipelineBackendDefault::GetCurrentPts() {
  if (!running_)
    return start_pts_.InMicroseconds();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interpolated_media_time =
      start_pts_ + (now - start_clock_) * rate_;
  return interpolated_media_time.InMicroseconds();
}

bool MediaPipelineBackendDefault::SetPlaybackRate(float rate) {
  DCHECK_GT(rate, 0.0f);
  start_pts_ = base::TimeDelta::FromMicroseconds(GetCurrentPts());
  start_clock_ = base::TimeTicks::Now();
  rate_ = rate;
  return true;
}

}  // namespace media
}  // namespace chromecast
