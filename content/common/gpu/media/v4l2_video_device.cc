// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "content/common/gpu/media/exynos_v4l2_video_device.h"
#include "content/common/gpu/media/tegra_v4l2_video_device.h"

namespace content {

V4L2Device::V4L2Device() {}

V4L2Device::~V4L2Device() {}

// static
scoped_ptr<V4L2Device> V4L2Device::Create(EGLContext egl_context) {
  DVLOG(3) << __PRETTY_FUNCTION__;

  scoped_ptr<ExynosV4L2Device> exynos_device(new ExynosV4L2Device());
  if (exynos_device->Initialize())
    return exynos_device.PassAs<V4L2Device>();

  scoped_ptr<TegraV4L2Device> tegra_device(new TegraV4L2Device(egl_context));
  if (tegra_device->Initialize())
    return tegra_device.PassAs<V4L2Device>();

  DLOG(ERROR) << "Failed to create V4L2Device";
  return scoped_ptr<V4L2Device>();
}
}  //  namespace content
