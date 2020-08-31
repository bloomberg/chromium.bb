// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/orientation/orientation_device_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "device/vr/orientation/orientation_device.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

VROrientationDeviceProvider::VROrientationDeviceProvider(
    mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider)
    : sensor_provider_(std::move(sensor_provider)) {}

VROrientationDeviceProvider::~VROrientationDeviceProvider() = default;

void VROrientationDeviceProvider::Initialize(
    base::RepeatingCallback<void(mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojo::PendingRemote<mojom::XRRuntime>)>
        add_device_callback,
    base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  if (device_ && device_->IsAvailable()) {
    add_device_callback.Run(device_->GetId(), device_->GetVRDisplayInfo(),
                            device_->BindXRRuntime());
    return;
  }

  if (!device_) {
    device_ = std::make_unique<VROrientationDevice>(
        sensor_provider_.get(),
        base::BindOnce(&VROrientationDeviceProvider::DeviceInitialized,
                       base::Unretained(this)));
    add_device_callback_ = add_device_callback;
    initialized_callback_ = std::move(initialization_complete);
  }
}

bool VROrientationDeviceProvider::Initialized() {
  return initialized_;
}

void VROrientationDeviceProvider::DeviceInitialized() {
  // This should only be called after the device is initialized.
  DCHECK(device_);
  // This should only be called once.
  DCHECK(!initialized_);

  // If the device successfully connected to the orientation APIs, provide it.
  if (device_->IsAvailable()) {
    add_device_callback_.Run(device_->GetId(), device_->GetVRDisplayInfo(),
                             device_->BindXRRuntime());
  }

  initialized_ = true;
  std::move(initialized_callback_).Run();
}

}  // namespace device
