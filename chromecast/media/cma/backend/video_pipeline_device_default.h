// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_

#include <vector>

#include "base/macros.h"
#include "chromecast/media/cma/backend/video_pipeline_device.h"
#include "chromecast/public/media/decoder_config.h"

namespace chromecast {
namespace media {

class MediaClockDevice;
class MediaComponentDeviceDefault;

class VideoPipelineDeviceDefault : public VideoPipelineDevice {
 public:
  explicit VideoPipelineDeviceDefault(MediaClockDevice* media_clock_device);
  ~VideoPipelineDeviceDefault() override;

  // VideoPipelineDevice implementation.
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
  void SetVideoClient(const VideoClient& client) override;
  bool SetConfig(const VideoConfig& config) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceDefault> pipeline_;

  VideoConfig config_;
  std::vector<uint8_t> config_extra_data_;

  DISALLOW_COPY_AND_ASSIGN(VideoPipelineDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_

