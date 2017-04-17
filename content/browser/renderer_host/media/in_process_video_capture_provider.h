// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_SYSTEM_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_SYSTEM_H_

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "media/capture/video/video_capture_system.h"

namespace content {

class CONTENT_EXPORT InProcessVideoCaptureProvider
    : public VideoCaptureProvider {
 public:
  InProcessVideoCaptureProvider(
      std::unique_ptr<media::VideoCaptureSystem> video_capture_system,
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);
  ~InProcessVideoCaptureProvider() override;

  void GetDeviceInfosAsync(
      const base::Callback<void(
          const std::vector<media::VideoCaptureDeviceInfo>&)>& result_callback)
      override;

  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

 private:
  const std::unique_ptr<media::VideoCaptureSystem> video_capture_system_;
  // The message loop of media stream device thread, where VCD's live.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_IN_PROCESS_VIDEO_CAPTURE_SYSTEM_H_
