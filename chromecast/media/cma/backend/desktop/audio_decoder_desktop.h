// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_AUDIO_DECODER_DESKTOP_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_AUDIO_DECODER_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

class MediaSinkDesktop;

class AudioDecoderDesktop : public MediaPipelineBackend::AudioDecoder {
 public:
  AudioDecoderDesktop();
  ~AudioDecoderDesktop() override;

  void Start(base::TimeDelta start_pts);
  void Stop();
  void SetPlaybackRate(float rate);
  base::TimeDelta GetCurrentPts();

  // MediaPipelineBackend::AudioDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  MediaPipelineBackend::BufferStatus PushBuffer(
      CastDecoderBuffer* buffer) override;
  void GetStatistics(Statistics* statistics) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;

 private:
  Delegate* delegate_;
  std::unique_ptr<MediaSinkDesktop> sink_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderDesktop);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_DESKTOP_AUDIO_DECODER_DESKTOP_H_
