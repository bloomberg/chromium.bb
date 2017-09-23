// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

enum class AudioContentType;
class AudioDecoderWrapper;
class MediaPipelineBackendManager;

class MediaPipelineBackendWrapper : public MediaPipelineBackend {
 public:
  MediaPipelineBackendWrapper(const media::MediaPipelineDeviceParams& params,
                              MediaPipelineBackendManager* backend_manager);
  ~MediaPipelineBackendWrapper() override;

  void LogicalPause();
  void LogicalResume();

  // MediaPipelineBackend implementation:
  AudioDecoder* CreateAudioDecoder() override;
  VideoDecoder* CreateVideoDecoder() override;
  bool Initialize() override;
  bool Start(int64_t start_pts) override;
  void Stop() override;
  bool Pause() override;
  bool Resume() override;
  int64_t GetCurrentPts() override;
  bool SetPlaybackRate(float rate) override;

 private:
  void SetPlaying(bool playing);

  bool IsSfx() {
    return audio_stream_type_ ==
           media::MediaPipelineDeviceParams::kAudioStreamSoundEffects;
  }

  const std::unique_ptr<MediaPipelineBackend> backend_;
  MediaPipelineBackendManager* const backend_manager_;
  const MediaPipelineDeviceParams::AudioStreamType audio_stream_type_;
  const AudioContentType content_type_;

  std::unique_ptr<AudioDecoderWrapper> audio_decoder_;

  bool have_video_decoder_;
  bool playing_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_WRAPPER_H_
