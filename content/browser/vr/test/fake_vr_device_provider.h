// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_
#define CONTENT_BROWSER_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_

#include <vector>
#include "content/browser/vr/vr_device.h"
#include "content/browser/vr/vr_device_provider.h"

namespace content {

class FakeVRDeviceProvider : public VRDeviceProvider {
 public:
  FakeVRDeviceProvider();
  ~FakeVRDeviceProvider() override;

  // Adds devices to the provider with the given device, which will be
  // returned when GetDevices is queried.
  void AddDevice(VRDevice* device);
  void RemoveDevice(VRDevice* device);
  bool IsInitialized() { return initialized_; }

  void GetDevices(std::vector<VRDevice*>& devices) override;
  void Initialize() override;

 private:
  std::vector<VRDevice*> devices_;
  bool initialized_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_TEST_FAKE_VR_DEVICE_PROVIDER_H_
