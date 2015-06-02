// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_

#include <vector>

#include "base/macros.h"
#include "chromecast/media/cma/backend/audio_pipeline_device.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

class MediaClockDevice;
class MediaComponentDeviceDefault;

class AudioPipelineDeviceDefault : public AudioPipelineDevice {
 public:
  explicit AudioPipelineDeviceDefault(MediaClockDevice* media_clock_device);
  ~AudioPipelineDeviceDefault() override;

  // AudioPipelineDevice implementation.
  void SetClient(const Client& client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(base::TimeDelta time) override;
  FrameStatus PushFrame(
      const scoped_refptr<DecryptContext>& decrypt_context,
      const scoped_refptr<DecoderBufferBase>& buffer,
      const FrameStatusCB& completion_cb) override;
  base::TimeDelta GetRenderingTime() const override;
  base::TimeDelta GetRenderingDelay() const override;
  bool SetConfig(const AudioConfig& config) override;
  void SetStreamVolumeMultiplier(float multiplier) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceDefault> pipeline_;

  AudioConfig config_;
  std::vector<uint8_t> config_extra_data_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipelineDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_PIPELINE_DEVICE_DEFAULT_H_
