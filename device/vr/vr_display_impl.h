// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DISPLAY_IMPL_H
#define DEVICE_VR_VR_DISPLAY_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace device {

class VRServiceImpl;

// Browser process representation of a VRDevice within a WebVR site session
// (see VRServiceImpl). VRDisplayImpl receives/sends VR device events
// from/to mojom::VRDisplayClient (the render process representation of a VR
// device).
// VRDisplayImpl objects are owned by their respective VRServiceImpl instances.
class VRDisplayImpl : public mojom::VRDisplay {
 public:
  VRDisplayImpl(device::VRDevice* device,
                VRServiceImpl* service,
                mojom::VRServiceClient* service_client,
                mojom::VRDisplayInfoPtr display_info);
  ~VRDisplayImpl() override;

  virtual void OnChanged(mojom::VRDisplayInfoPtr vr_device_info);
  virtual void OnExitPresent();
  virtual void OnBlur();
  virtual void OnFocus();
  virtual void OnActivate(mojom::VRDisplayEventReason reason,
                          const base::Callback<void(bool)>& on_handled);
  virtual void OnDeactivate(mojom::VRDisplayEventReason reason);

 private:
  friend class VRDisplayImplTest;
  friend class VRServiceImpl;

  void RequestPresent(bool secure_origin,
                      mojom::VRSubmitFrameClientPtr submit_client,
                      mojom::VRPresentationProviderRequest request,
                      RequestPresentCallback callback) override;
  void ExitPresent() override;
  void GetNextMagicWindowPose(GetNextMagicWindowPoseCallback callback) override;

  void RequestPresentResult(RequestPresentCallback callback,
                            bool secure_origin,
                            bool success);

  mojo::Binding<mojom::VRDisplay> binding_;
  mojom::VRDisplayClientPtr client_;
  device::VRDevice* device_;
  VRServiceImpl* service_;

  base::WeakPtrFactory<VRDisplayImpl> weak_ptr_factory_;
};

}  // namespace device

#endif  //  DEVICE_VR_VR_DISPLAY_IMPL_H
