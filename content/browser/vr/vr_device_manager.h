// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_VR_DEVICE_MANAGER_H
#define CONTENT_BROWSER_VR_VR_DEVICE_MANAGER_H

#include <stdint.h>

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/browser/vr/vr_device.h"
#include "content/browser/vr/vr_device_provider.h"
#include "content/common/content_export.h"
#include "content/common/vr_service.mojom.h"
#include "mojo/common/weak_binding_set.h"

namespace content {

class VRDeviceManager : public VRService {
 public:
  ~VRDeviceManager() override;

  static void BindRequest(mojo::InterfaceRequest<VRService> request);

  // Returns the VRDeviceManager singleton.
  static VRDeviceManager* GetInstance();

  mojo::Array<VRDeviceInfoPtr> GetVRDevices();
  VRDevice* GetDevice(unsigned int index);

 private:
  friend class VRDeviceManagerTest;

  VRDeviceManager();
  // Constructor for testing.
  explicit VRDeviceManager(scoped_ptr<VRDeviceProvider> provider);

  static void SetInstance(VRDeviceManager* service);
  static bool HasInstance();

  void InitializeProviders();
  void RegisterProvider(scoped_ptr<VRDeviceProvider> provider);

  // VRService implementation
  void GetDevices(const GetDevicesCallback& callback) override;
  void GetSensorState(uint32_t index,
                      const GetSensorStateCallback& callback) override;
  void ResetSensor(uint32_t index) override;

  // Mojo connection error handler.
  void OnConnectionError();

  using ProviderList = std::vector<linked_ptr<VRDeviceProvider>>;
  ProviderList providers_;

  // Devices are owned by their providers.
  using DeviceMap = std::map<unsigned int, VRDevice*>;
  DeviceMap devices_;

  bool vr_initialized_;

  mojo::WeakBindingSet<VRService> bindings_;

  // For testing. If true will not delete self when consumer count reaches 0.
  bool keep_alive_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_VR_DEVICE_MANAGER_H
