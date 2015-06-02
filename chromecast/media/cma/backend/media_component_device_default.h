// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_

#include <list>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/cma/backend/media_clock_device.h"
#include "chromecast/media/cma/backend/media_component_device.h"

namespace chromecast {
namespace media {

class MediaComponentDeviceDefault : public MediaComponentDevice {
 public:
  explicit MediaComponentDeviceDefault(MediaClockDevice* media_clock_device);
  ~MediaComponentDeviceDefault() override;

  // MediaComponentDevice implementation.
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

  MediaClockDevice* const media_clock_device_;
  Client client_;

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
  scoped_refptr<DecoderBufferBase> pending_buffer_;
  FrameStatusCB frame_pushed_cb_;

  base::WeakPtr<MediaComponentDeviceDefault> weak_this_;
  base::WeakPtrFactory<MediaComponentDeviceDefault> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaComponentDeviceDefault);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_DEFAULT_H_
