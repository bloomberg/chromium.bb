// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_SINGLE_PLANE_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_SINGLE_PLANE_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"
#include "media/capture/video/video_capture_device.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// V4L2 specifics for SPLANE API.
class V4L2CaptureDelegateSinglePlane final : public V4L2CaptureDelegate {
 public:
  V4L2CaptureDelegateSinglePlane(
      const VideoCaptureDevice::Name& device_name,
      const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
      int power_line_frequency)
      : V4L2CaptureDelegate(device_name,
                            v4l2_task_runner,
                            power_line_frequency) {}

 private:
  // BufferTracker derivation to implement construction semantics for SPLANE.
  class BufferTrackerSPlane final : public BufferTracker {
   public:
    bool Init(int fd, const v4l2_buffer& buffer) override;

   private:
    ~BufferTrackerSPlane() override {}
  };

  ~V4L2CaptureDelegateSinglePlane() override {}

  // V4L2CaptureDelegate virtual methods implementation.
  scoped_refptr<BufferTracker> CreateBufferTracker() const override;
  bool FillV4L2Format(v4l2_format* format,
                      uint32_t width,
                      uint32_t height,
                      uint32_t pixelformat_fourcc) const override;
  void FinishFillingV4L2Buffer(v4l2_buffer* buffer) const override;
  void SetPayloadSize(const scoped_refptr<BufferTracker>& buffer_tracker,
                      const v4l2_buffer& buffer) const override;
  void SendBuffer(const scoped_refptr<BufferTracker>& buffer_tracker,
                  const v4l2_format& format) const override;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_LINUX_V4L2_CAPTURE_DELEGATE_MULTI_PLANE_H_
