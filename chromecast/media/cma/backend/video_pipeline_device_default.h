// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/video_pipeline_device.h"

namespace chromecast {
namespace media {

class MediaClockDevice;
class MediaComponentDeviceDefault;
struct MediaPipelineDeviceParams;

class VideoPipelineDeviceDefault : public VideoPipelineDevice {
 public:
  VideoPipelineDeviceDefault(const MediaPipelineDeviceParams& params,
                             MediaClockDevice* media_clock_device);
  ~VideoPipelineDeviceDefault() override;

  // VideoPipelineDevice implementation.
  void SetClient(Client* client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(int64_t time_microseconds) override;
  FrameStatus PushFrame(DecryptContext* decrypt_context,
                        CastDecoderBuffer* buffer,
                        FrameStatusCB* completion_cb) override;
  RenderingDelay GetRenderingDelay() const override;
  void SetVideoClient(VideoClient* client) override;
  bool SetConfig(const VideoConfig& config) override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  scoped_ptr<MediaComponentDeviceDefault> pipeline_;

  VideoConfig config_;
  std::vector<uint8_t> config_extra_data_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(VideoPipelineDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_VIDEO_PIPELINE_DEVICE_DEFAULT_H_
