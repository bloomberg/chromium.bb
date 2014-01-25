// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "content/common/gpu/media/exynos_v4l2_video_device.h"

namespace content {

V4L2Device::V4L2Device() {}

V4L2Device::~V4L2Device() {}

// static
scoped_ptr<V4L2Device> V4L2Device::Create() {
  DVLOG(3) << __PRETTY_FUNCTION__;

  scoped_ptr<ExynosV4L2Device> device(new ExynosV4L2Device());
  if (!device->Initialize()) {
    // TODO(shivdasp): Try and create other V4L2Devices.
    device.reset(NULL);
  }
  return device.PassAs<V4L2Device>();
}
}  //  namespace content
