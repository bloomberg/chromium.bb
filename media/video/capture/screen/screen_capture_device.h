// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SCREEN_CAPTURER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SCREEN_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/video/capture/screen/screen_capturer.h"
#include "media/video/capture/video_capture_device.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {

// ScreenCaptureDevice implements VideoCaptureDevice for the screen. It's
// essentially an adapter between ScreenCapturer and VideoCaptureDevice.
class MEDIA_EXPORT ScreenCaptureDevice : public VideoCaptureDevice {
 public:
  explicit ScreenCaptureDevice(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  virtual ~ScreenCaptureDevice();

  // Helper used in tests to supply a fake capturer.
  void SetScreenCapturerForTest(
      scoped_ptr<media::ScreenCapturer> capturer);

  // VideoCaptureDevice interface.
  virtual void Allocate(int width, int height,
                        int frame_rate,
                        EventHandler* event_handler) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void DeAllocate() OVERRIDE;
  virtual const Name& device_name() OVERRIDE;

 private:
  class Core;
  scoped_refptr<Core> core_;
  Name name_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureDevice);
};

}  // namespace media

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SCREEN_CAPTURER_H_
