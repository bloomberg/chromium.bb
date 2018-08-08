// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_device_provider.h"

#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/vr/isolated_gamepad_data_fetcher.h"
#include "device/vr/oculus/oculus_device.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"

namespace device {

OculusVRDeviceProvider::OculusVRDeviceProvider() : initialized_(false) {}

OculusVRDeviceProvider::~OculusVRDeviceProvider() {
  // Removing gamepad factory corresponding to VRDeviceId::OCULUS_DEVICE_ID,
  // added during initialization.
  device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device::GAMEPAD_SOURCE_OCULUS);
}

void OculusVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojom::XRRuntimePtr)> add_device_callback,
    base::RepeatingCallback<void(device::mojom::XRDeviceId)>
        remove_device_callback,
    base::OnceClosure initialization_complete) {
  CreateDevice();
  if (device_) {
    add_device_callback.Run(device::mojom::XRDeviceId::OCULUS_DEVICE_ID,
                            device_->GetVRDisplayInfo(),
                            device_->BindXRRuntimePtr());

    // Removed in our destructor, as VRDeviceId::OCULUS_DEVICE_ID corresponds to
    // device::GAMEPAD_SOURCE_OCULUS.
    GamepadDataFetcherManager::GetInstance()->AddFactory(
        new IsolatedGamepadDataFetcher::Factory(
            device::mojom::XRDeviceId::OCULUS_DEVICE_ID,
            device_->BindGamepadFactory()));
  }
  initialized_ = true;
  std::move(initialization_complete).Run();
}

void OculusVRDeviceProvider::CreateDevice() {
  // TODO(billorr): Check for headset presence without starting runtime.
  device_ = std::make_unique<OculusDevice>();

  // If the device failed to inialize, don't return it.
  if (!device_->IsInitialized()) {
    device_ = nullptr;
  }
}

bool OculusVRDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
