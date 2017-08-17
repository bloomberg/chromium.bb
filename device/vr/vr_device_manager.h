// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_MANAGER_H
#define DEVICE_VR_VR_DEVICE_MANAGER_H

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "device/vr/vr_service_impl.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace device {

// Singleton used to provide the platform's VR devices to VRServiceImpl
// instances.
class DEVICE_VR_EXPORT VRDeviceManager {
 public:
  virtual ~VRDeviceManager();

  // Returns the VRDeviceManager singleton.
  static VRDeviceManager* GetInstance();

  // Adds a listener for device manager events. VRDeviceManager does not own
  // this object.
  // Automatically connects all currently available VR devices by querying
  // the device providers and, for each returned device, calling
  // VRServiceImpl::ConnectDevice.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  unsigned int GetNumberOfConnectedDevices();

  VRDevice* GetDevice(unsigned int index);

 private:
  friend class VRDeviceManagerTest;
  friend class VRDisplayImplTest;
  friend class VRServiceImplTest;

  VRDeviceManager();
  // Constructor for testing.
  explicit VRDeviceManager(std::unique_ptr<VRDeviceProvider> provider);

  void InitializeProviders();
  void RegisterProvider(std::unique_ptr<VRDeviceProvider> provider);

  using ProviderList = std::vector<std::unique_ptr<VRDeviceProvider>>;
  ProviderList providers_;

  // Devices are owned by their providers.
  using DeviceMap = std::map<unsigned int, VRDevice*>;
  DeviceMap devices_;

  bool vr_initialized_ = false;

  std::set<VRServiceImpl*> services_;

  // For testing. If true will not delete self when consumer count reaches 0.
  bool keep_alive_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_VR_DEVICE_MANAGER_H
