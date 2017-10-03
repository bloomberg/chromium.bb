// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_DEVICE_MANAGER_H_
#define CHROME_BROWSER_VR_SERVICE_VR_DEVICE_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/vr/service/vr_service_impl.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace vr {

// Singleton used to provide the platform's VR devices to VRServiceImpl
// instances.
class VRDeviceManager {
 public:
  virtual ~VRDeviceManager();

  // Returns the VRDeviceManager singleton.
  static VRDeviceManager* GetInstance();
  static bool HasInstance();

  // Adds a listener for device manager events. VRDeviceManager does not own
  // this object.
  // Automatically connects all currently available VR devices by querying
  // the device providers and, for each returned device, calling
  // VRServiceImpl::ConnectDevice.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  device::VRDevice* GetDevice(unsigned int index);

 protected:
  using ProviderList = std::vector<std::unique_ptr<device::VRDeviceProvider>>;

  // Used by tests to supply providers.
  explicit VRDeviceManager(ProviderList providers);

  size_t NumberOfConnectedServices();

 private:
  void InitializeProviders();

  ProviderList providers_;

  // Devices are owned by their providers.
  using DeviceMap = std::map<unsigned int, device::VRDevice*>;
  DeviceMap devices_;

  bool vr_initialized_ = false;

  std::set<VRServiceImpl*> services_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManager);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_DEVICE_MANAGER_H_
