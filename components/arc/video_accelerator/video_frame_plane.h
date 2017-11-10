// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_FRAME_PLANE_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_FRAME_PLANE_H_

#include <stdint.h>

namespace arc {

struct VideoFramePlane {
  int32_t offset;  // in bytes
  int32_t stride;  // in bytes
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_VIDEO_FRAME_PLANE_H_
