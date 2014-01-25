// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the implementation of ExynosV4L2Device used on
// Exynos platform.

#ifndef CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_
#define CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_

#include "content/common/gpu/media/v4l2_video_device.h"

namespace content {

class ExynosV4L2Device : public V4L2Device {
 public:
  ExynosV4L2Device();
  virtual ~ExynosV4L2Device();

  // V4L2Device implementation.
  int Ioctl(int request, void* arg) OVERRIDE;
  bool Poll(bool poll_device, bool* event_pending) OVERRIDE;
  bool SetDevicePollInterrupt() OVERRIDE;
  bool ClearDevicePollInterrupt() OVERRIDE;
  void* Mmap(void* addr,
             unsigned int len,
             int prot,
             int flags,
             unsigned int offset) OVERRIDE;
  void Munmap(void* addr, unsigned int len) OVERRIDE;

  // Does all the initialization of device fds, returns true on success.
  bool Initialize();

 private:
  // The actual device fd.
  int device_fd_;

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  int device_poll_interrupt_fd_;

  DISALLOW_COPY_AND_ASSIGN(ExynosV4L2Device);
};
}  //  namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_EXYNOS_V4L2_VIDEO_DEVICE_H_
