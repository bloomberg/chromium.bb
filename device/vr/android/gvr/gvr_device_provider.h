// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H

#include <memory>

#include "base/macros.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"

namespace device {

class GvrDelegateProvider;
class GvrDevice;

class DEVICE_VR_EXPORT GvrDeviceProvider : public VRDeviceProvider {
 public:
  GvrDeviceProvider();
  ~GvrDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

  device::GvrDelegateProvider* GetDelegateProvider();

  GvrDevice* Device() { return vr_device_.get(); }

 private:
  void Initialize(device::GvrDelegateProvider* provider);
  std::unique_ptr<GvrDevice> vr_device_;

  DISALLOW_COPY_AND_ASSIGN(GvrDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
