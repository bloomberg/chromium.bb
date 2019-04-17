// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_VIDEO_CAPTURE_DEPENDENCIES_H_
#define CONTENT_BROWSER_GPU_VIDEO_CAPTURE_DEPENDENCIES_H_

#include "content/common/content_export.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"

namespace content {

// Browser-process-provided GPU dependencies for video capture.
class CONTENT_EXPORT VideoCaptureDependencies {
 public:
  static void CreateJpegDecodeAccelerator(
      media::mojom::MjpegDecodeAcceleratorRequest accelerator);
#if defined(OS_CHROMEOS)
  static void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest accelerator);
#endif  // defined(OS_CHROMEOS)
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_VIDEO_CAPTURE_DEPENDENCIES_H_
