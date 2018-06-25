// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DISPLAY_IMPL_H
#define DEVICE_VR_VR_DISPLAY_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"

namespace device {

class VRDeviceBase;

// Browser process representation of a VRDevice within a WebVR site session
// (see VRServiceImpl). VRDisplayImpl receives/sends VR device events
// from/to mojom::VRDisplayClient (the render process representation of a VR
// device).
// VRDisplayImpl objects are owned by their respective VRServiceImpl instances.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDisplayImpl : public mojom::VRMagicWindowProvider,
                                       public XrSessionController {
 public:
  VRDisplayImpl(VRDevice* device,
                mojom::VRServiceClient* service_client,
                mojom::VRDisplayInfoPtr display_info,
                mojom::VRDisplayHostPtr display_host,
                mojom::VRDisplayClientRequest client_request);
  ~VRDisplayImpl() override;

  // XrSessionController
  void SetFrameDataRestricted(bool paused) override;
  void StopSession() override;

 private:
  // mojom::VRMagicWindowProvider
  void GetFrameData(GetFrameDataCallback callback) override;
  void UpdateSessionGeometry(const gfx::Size& frame_size,
                             display::Display::Rotation rotation) override;
  void RequestHitTest(mojom::XRRayPtr ray,
                      RequestHitTestCallback callback) override;

  mojo::Binding<mojom::VRMagicWindowProvider> binding_;
  device::VRDeviceBase* device_;

  bool restrict_frame_data_ = true;

  // TODO(offenwanger) When device tracks it's own sessions, let it track this
  // data.
  gfx::Size session_frame_size_ = gfx::Size(0, 0);
  display::Display::Rotation session_rotation_ = display::Display::ROTATE_0;
};

}  // namespace device

#endif  //  DEVICE_VR_VR_DISPLAY_IMPL_H
