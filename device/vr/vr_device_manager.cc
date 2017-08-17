// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "device/vr/features/features.h"

#if defined(OS_ANDROID)
#include "device/vr/android/gvr/gvr_device_provider.h"
#endif

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device_provider.h"
#endif

namespace device {

namespace {
VRDeviceManager* g_vr_device_manager = nullptr;
}

VRDeviceManager::VRDeviceManager() : keep_alive_(false) {
// Register VRDeviceProviders for the current platform
#if defined(OS_ANDROID)
  RegisterProvider(base::MakeUnique<GvrDeviceProvider>());
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  RegisterProvider(base::MakeUnique<OpenVRDeviceProvider>());
#endif
}

VRDeviceManager::VRDeviceManager(std::unique_ptr<VRDeviceProvider> provider)
    : keep_alive_(true) {
  thread_checker_.DetachFromThread();
  RegisterProvider(std::move(provider));
  CHECK(!g_vr_device_manager);
  g_vr_device_manager = this;
}

VRDeviceManager::~VRDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_vr_device_manager = nullptr;
}

VRDeviceManager* VRDeviceManager::GetInstance() {
  if (!g_vr_device_manager)
    g_vr_device_manager = new VRDeviceManager();
  return g_vr_device_manager;
}

void VRDeviceManager::AddService(VRServiceImpl* service) {
  // Loop through any currently active devices and send Connected messages to
  // the service. Future devices that come online will send a Connected message
  // when they are created.
  DCHECK(thread_checker_.CalledOnValidThread());

  InitializeProviders();

  std::vector<VRDevice*> devices;
  for (const auto& provider : providers_) {
    provider->GetDevices(&devices);
  }

  for (auto* device : devices) {
    if (device->id() == VR_DEVICE_LAST_ID) {
      continue;
    }

    if (devices_.find(device->id()) == devices_.end()) {
      devices_[device->id()] = device;
    }

    service->ConnectDevice(device);
  }

  services_.insert(service);
}

void VRDeviceManager::RemoveService(VRServiceImpl* service) {
  services_.erase(service);

  if (services_.empty() && !keep_alive_) {
    // Delete the device manager when it has no active connections.
    delete g_vr_device_manager;
  }
}

unsigned int VRDeviceManager::GetNumberOfConnectedDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return static_cast<unsigned int>(devices_.size());
}

VRDevice* VRDeviceManager::GetDevice(unsigned int index) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (index == 0) {
    return NULL;
  }

  DeviceMap::iterator iter = devices_.find(index);
  if (iter == devices_.end()) {
    return nullptr;
  }
  return iter->second;
}

void VRDeviceManager::InitializeProviders() {
  if (vr_initialized_) {
    return;
  }

  for (const auto& provider : providers_) {
    provider->Initialize();
  }

  vr_initialized_ = true;
}

void VRDeviceManager::RegisterProvider(
    std::unique_ptr<VRDeviceProvider> provider) {
  providers_.push_back(std::move(provider));
}

}  // namespace device
