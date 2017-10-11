// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_AUDIO_DECODER_FUCHSIA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_AUDIO_DECODER_FUCHSIA_H_

#include "base/threading/thread_checker.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

class AudioDecoderFuchsia : public MediaPipelineBackend::AudioDecoder {
 public:
  AudioDecoderFuchsia();
  ~AudioDecoderFuchsia();

  void Initialize();
  bool Start(int64_t start_pts);
  void Stop();
  bool Pause();
  bool Resume();
  bool SetPlaybackRate(float rate);

  int64_t GetCurrentPts() const;

  // AudioDecoder interface.
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;

  // Decoder interface.
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(CastDecoderBuffer* buffer) override;

 private:
  Statistics stats_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderFuchsia);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_AUDIO_DECODER_FUCHSIA_H_
