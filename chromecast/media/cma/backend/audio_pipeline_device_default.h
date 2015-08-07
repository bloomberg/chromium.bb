// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

class MediaClockDevice;
class MediaComponentDeviceDefault;
struct MediaPipelineDeviceParams;

class AudioPipelineDeviceDefault : public AudioPipelineDevice {
 public:
  AudioPipelineDeviceDefault(const MediaPipelineDeviceParams& params,
                             MediaClockDevice* media_clock_device);
  ~AudioPipelineDeviceDefault() override;

  // AudioPipelineDevice implementation.
  void SetClient(Client* client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(int64_t time_microseconds) override;
  FrameStatus PushFrame(DecryptContext* decrypt_context,
                        CastDecoderBuffer* buffer,
                        FrameStatusCB* completion_cb) override;
  RenderingDelay GetRenderingDelay() const override;
  bool SetConfig(const AudioConfig& config) override;
  void SetStreamVolumeMultiplier(float multiplier) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceDefault> pipeline_;

  AudioConfig config_;
  std::vector<uint8_t> config_extra_data_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(AudioPipelineDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_
