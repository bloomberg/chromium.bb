// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

namespace device {

ARCoreDeviceProvider::ARCoreDeviceProvider() = default;

ARCoreDeviceProvider::~ARCoreDeviceProvider() = default;

void ARCoreDeviceProvider::Initialize(
    base::RepeatingCallback<void(unsigned int,
                                 mojom::VRDisplayInfoPtr,
                                 mojom::XRRuntimePtr)> add_device_callback,
    base::RepeatingCallback<void(unsigned int)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  arcore_device_ = base::WrapUnique(new ARCoreDevice());
  add_device_callback.Run(arcore_device_->GetId(),
                          arcore_device_->GetVRDisplayInfo(),
                          arcore_device_->BindXRRuntimePtr());
  initialized_ = true;
  std::move(initialization_complete).Run();
}

bool ARCoreDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
