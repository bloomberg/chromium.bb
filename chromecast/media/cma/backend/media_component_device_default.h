// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_

#include <list>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/media_component_device.h"

namespace chromecast {
class TaskRunner;

namespace media {
struct MediaPipelineDeviceParams;

class MediaComponentDeviceDefault : public MediaComponentDevice {
 public:
  MediaComponentDeviceDefault(const MediaPipelineDeviceParams& params,
                              MediaClockDevice* media_clock_device);
  ~MediaComponentDeviceDefault() override;

  // MediaComponentDevice implementation.
  void SetClient(Client* client) override;
  State GetState() const override;
  bool SetState(State new_state) override;
  bool SetStartPts(int64_t time_microseconds) override;
  FrameStatus PushFrame(DecryptContext* decrypt_context,
                        CastDecoderBuffer* buffer,
                        FrameStatusCB* completion_cb) override;
  RenderingDelay GetRenderingDelay() const override;
  bool GetStatistics(Statistics* stats) const override;

 private:
  struct DefaultDecoderBuffer {
    DefaultDecoderBuffer();
    ~DefaultDecoderBuffer();

    // Buffer size.
    size_t size;

    // Presentation timestamp.
    base::TimeDelta pts;
  };

  void RenderTask();

  TaskRunner* task_runner_;
  MediaClockDevice* const media_clock_device_;
  scoped_ptr<Client> client_;

  State state_;

  // Indicate whether the end of stream has been received.
  bool is_eos_;

  // Media time of the last rendered audio sample.
  base::TimeDelta rendering_time_;

  // Frame decoded/rendered since the pipeline left the idle state.
  uint64 decoded_frame_count_;
  uint64 decoded_byte_count_;

  // List of frames not rendered yet.
  std::list<DefaultDecoderBuffer> frames_;

  // Indicate whether there is a scheduled rendering task.
  bool scheduled_rendering_task_;

  // Pending frame.
  scoped_ptr<CastDecoderBuffer> pending_buffer_;
  scoped_ptr<FrameStatusCB> frame_pushed_cb_;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<MediaComponentDeviceDefault> weak_this_;
  base::WeakPtrFactory<MediaComponentDeviceDefault> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaComponentDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_
