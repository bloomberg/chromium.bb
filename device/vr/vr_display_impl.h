// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DISPLAY_IMPL_H
#define DEVICE_VR_VR_DISPLAY_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRDevice;
class VRDeviceBase;

// Browser process representation of a VRDevice within a WebVR site session
// (see VRServiceImpl). VRDisplayImpl receives/sends VR device events
// from/to mojom::VRDisplayClient (the render process representation of a VR
// device).
// VRDisplayImpl objects are owned by their respective VRServiceImpl instances.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDisplayImpl : public mojom::VRMagicWindowProvider {
 public:
  VRDisplayImpl(VRDevice* device,
                mojom::VRServiceClient* service_client,
                mojom::VRDisplayInfoPtr display_info,
                mojom::VRDisplayHostPtr display_host,
                bool in_frame_focused);
  ~VRDisplayImpl() override;

  virtual void OnChanged(mojom::VRDisplayInfoPtr vr_device_info);
  virtual void OnExitPresent();
  virtual void OnBlur();
  virtual void OnFocus();
  virtual void OnActivate(mojom::VRDisplayEventReason reason,
                          base::Callback<void(bool)> on_handled);
  virtual void OnDeactivate(mojom::VRDisplayEventReason reason);

  void SetListeningForActivate(bool listening);
  void SetInFocusedFrame(bool in_focused_frame);
  virtual bool ListeningForActivate();
  virtual bool InFocusedFrame();

  void RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                      mojom::VRPresentationProviderRequest request,
                      mojom::VRRequestPresentOptionsPtr options,
                      mojom::VRDisplayHost::RequestPresentCallback callback);
  void ExitPresent();

 private:
  // mojom::VRMagicWindowProvider
  void GetPose(GetPoseCallback callback) override;

  mojo::Binding<mojom::VRMagicWindowProvider> binding_;
  mojom::VRDisplayClientPtr client_;
  device::VRDeviceBase* device_;
  bool listening_for_activate_ = false;
  bool in_focused_frame_ = false;
};

}  // namespace device

#endif  //  DEVICE_VR_VR_DISPLAY_IMPL_H
