// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BUFFER_PRESENTED_PARAMS_MAC_H_
#define CONTENT_COMMON_BUFFER_PRESENTED_PARAMS_MAC_H_

#include "base/time/time.h"
#include "gpu/ipc/common/surface_handle.h"

namespace content {

struct BufferPresentedParams {
  BufferPresentedParams();
  ~BufferPresentedParams();

  gpu::SurfaceHandle surface_handle;
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;
};

}  // namespace content

#endif  // CONTENT_COMMON_BUFFER_PRESENTED_PARAMS_MAC_H_
