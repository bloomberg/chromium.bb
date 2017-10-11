// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_MEDIA_PIPELINE_BACKEND_FUCHSIA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_MEDIA_PIPELINE_BACKEND_FUCHSIA_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

class AudioDecoderFuchsia;
class VideoDecoderNull;

class MediaPipelineBackendFuchsia : public MediaPipelineBackend {
 public:
  explicit MediaPipelineBackendFuchsia(const MediaPipelineDeviceParams& params);
  ~MediaPipelineBackendFuchsia() override;

  // MediaPipelineBackend interface.
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  bool SetPlaybackRate(float rate) override;
  int64_t GetCurrentPts() override;

 private:
  // State variable for DCHECKing caller correctness.
  enum State {
    kStateUninitialized,
    kStateInitialized,
    kStatePlaying,
    kStatePaused,
  };

  const MediaPipelineDeviceParams params_;

  State state_ = kStateUninitialized;

  std::unique_ptr<AudioDecoderFuchsia> audio_decoder_;
  std::unique_ptr<VideoDecoderNull> video_decoder_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendFuchsia);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_FUCHSIA_MEDIA_PIPELINE_BACKEND_FUCHSIA_H_
