// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MEDIA_PIPELINE_BACKEND_ALSA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MEDIA_PIPELINE_BACKEND_ALSA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {
class AudioDecoderAlsa;
class VideoDecoderNull;

class MediaPipelineBackendAlsa : public MediaPipelineBackend {
 public:
  explicit MediaPipelineBackendAlsa(const MediaPipelineDeviceParams& params);
  ~MediaPipelineBackendAlsa() override;

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  bool SetPlaybackRate(float rate) override;
  int64_t GetCurrentPts() override;

  bool Primary() const;
  std::string DeviceId() const;
  AudioContentType ContentType() const;
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner() const;

 private:
  // State variable for DCHECKing caller correctness.
  enum State {
    kStateUninitialized,
    kStateInitialized,
    kStatePlaying,
    kStatePaused,
  };
  State state_;

  const MediaPipelineDeviceParams params_;
  std::unique_ptr<VideoDecoderNull> video_decoder_;
  std::unique_ptr<AudioDecoderAlsa> audio_decoder_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendAlsa);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ALSA_MEDIA_PIPELINE_BACKEND_ALSA_H_
