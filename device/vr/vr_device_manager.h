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
#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "device/vr/vr_client_dispatcher.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"
#include "device/vr/vr_service.mojom.h"
#include "device/vr/vr_service_impl.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace device {

class VRDeviceManager : public VRClientDispatcher {
 public:
  DEVICE_VR_EXPORT virtual ~VRDeviceManager();

  // Returns the VRDeviceManager singleton.
  static VRDeviceManager* GetInstance();

  // Gets a VRDevice instance if the specified service is allowed to access it.
  DEVICE_VR_EXPORT static VRDevice* GetAllowedDevice(VRServiceImpl* service,
                                                     unsigned int index);

  // Adds a listener for device manager events. VRDeviceManager does not own
  // this object.
  void AddService(VRServiceImpl* service);
  void RemoveService(VRServiceImpl* service);

  DEVICE_VR_EXPORT mojo::Array<VRDisplayPtr> GetVRDevices();

  // Manage presentation to only allow a single service and device at a time.
  DEVICE_VR_EXPORT bool RequestPresent(VRServiceImpl* service,
                                       unsigned int index,
                                       bool secure_origin);
  DEVICE_VR_EXPORT void ExitPresent(VRServiceImpl* service, unsigned int index);
  void SubmitFrame(VRServiceImpl* service, unsigned int index, VRPosePtr pose);

  // VRClientDispatcher implementation
  void OnDeviceChanged(VRDisplayPtr device) override;
  void OnDeviceConnectionStatusChanged(VRDevice* device,
                                       bool is_connected) override;
  void OnPresentEnded(VRDevice* device) override;

 private:
  friend class VRDeviceManagerTest;
  friend class VRServiceImplTest;

  VRDeviceManager();
  // Constructor for testing.
  DEVICE_VR_EXPORT explicit VRDeviceManager(
      std::unique_ptr<VRDeviceProvider> provider);

  DEVICE_VR_EXPORT VRDevice* GetDevice(unsigned int index);

  static void SetInstance(VRDeviceManager* service);
  static bool HasInstance();

  void InitializeProviders();
  void RegisterProvider(std::unique_ptr<VRDeviceProvider> provider);

  void SchedulePollEvents();
  void PollEvents();
  void StopSchedulingPollEvents();

  using ProviderList = std::vector<linked_ptr<VRDeviceProvider>>;
  ProviderList providers_;

  // Devices are owned by their providers.
  using DeviceMap = std::map<unsigned int, VRDevice*>;
  DeviceMap devices_;

  bool vr_initialized_;

  using ServiceList = std::vector<VRServiceImpl*>;
  ServiceList services_;

  // Only one service and device is allowed to present at a time.
  VRServiceImpl* presenting_service_;
  VRDevice* presenting_device_;

  // For testing. If true will not delete self when consumer count reaches 0.
  bool keep_alive_;

  bool has_scheduled_poll_;

  base::ThreadChecker thread_checker_;

  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VR_VR_DEVICE_MANAGER_H
