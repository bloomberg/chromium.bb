// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INTERFACE_H_

#include <memory>
#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/display/display.h"

namespace device {

// This allows a real or fake implementation of ARCore to
// be used as appropriate (i.e. for testing).
class ARCoreInterface {
 public:
  virtual ~ARCoreInterface() = default;
  virtual void SetCameraTexture(uint texture) = 0;
  virtual void SetDisplayGeometry(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation) = 0;
  virtual void TransformDisplayUvCoords(int num_elements,
                                        const float* uvs_in,
                                        float* uvs_out) = 0;
  virtual void GetProjectionMatrix(float* matrix_out,
                                   float near,
                                   float far) = 0;
  virtual mojom::VRPosePtr Update() = 0;
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INTERFACE_H_
