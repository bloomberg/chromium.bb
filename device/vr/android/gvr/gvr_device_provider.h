// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H

#include <memory>

#include "base/macros.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"

namespace device {

class GvrDeviceProviderDelegate;

class GvrDeviceProvider : public VRDeviceProvider, public GvrDelegateClient {
 public:
  GvrDeviceProvider();
  ~GvrDeviceProvider() override;

  void GetDevices(std::vector<VRDevice*>* devices) override;
  void Initialize() override;

  // GvrDelegateClient
  void OnDelegateInitialized(GvrDelegate* delegate) override;
  void OnDelegateShutdown() override;

 private:
  std::unique_ptr<VRDevice> vr_device_;
  std::unique_ptr<GvrDeviceProviderDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(GvrDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_PROVIDER_H
