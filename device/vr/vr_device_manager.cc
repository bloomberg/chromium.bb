// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "device/vr/android/gvr/gvr_device_provider.h"
#endif

namespace device {

namespace {
VRDeviceManager* g_vr_device_manager = nullptr;
}

VRDeviceManager::VRDeviceManager()
    : vr_initialized_(false), keep_alive_(false) {
// Register VRDeviceProviders for the current platform
#if defined(OS_ANDROID)
  RegisterProvider(base::WrapUnique(new GvrDeviceProvider()));
#endif
}

VRDeviceManager::VRDeviceManager(std::unique_ptr<VRDeviceProvider> provider)
    : vr_initialized_(false), keep_alive_(true) {
  thread_checker_.DetachFromThread();
  RegisterProvider(std::move(provider));
  SetInstance(this);
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

void VRDeviceManager::SetInstance(VRDeviceManager* instance) {
  // Unit tests can create multiple instances but only one should exist at any
  // given time so g_vr_device_manager should only go from nullptr to
  // non-nullptr and vica versa.
  CHECK_NE(!!instance, !!g_vr_device_manager);
  g_vr_device_manager = instance;
}

bool VRDeviceManager::HasInstance() {
  // For testing. Checks to see if a VRDeviceManager instance is active.
  return !!g_vr_device_manager;
}

void VRDeviceManager::AddService(VRServiceImpl* service) {
  services_.push_back(service);

  // Ensure that the device providers are initialized
  InitializeProviders();
}

void VRDeviceManager::RemoveService(VRServiceImpl* service) {
  services_.erase(std::remove(services_.begin(), services_.end(), service),
                  services_.end());

  if (services_.empty() && !keep_alive_) {
    // Delete the device manager when it has no active connections.
    delete g_vr_device_manager;
  }
}

mojo::Array<VRDisplayPtr> VRDeviceManager::GetVRDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  InitializeProviders();

  std::vector<VRDevice*> devices;
  for (const auto& provider : providers_)
    provider->GetDevices(&devices);

  mojo::Array<VRDisplayPtr> out_devices;
  for (auto* device : devices) {
    if (device->id() == VR_DEVICE_LAST_ID)
      continue;

    if (devices_.find(device->id()) == devices_.end())
      devices_[device->id()] = device;

    VRDisplayPtr vr_device_info = device->GetVRDevice();
    if (vr_device_info.is_null())
      continue;

    vr_device_info->index = device->id();
    out_devices.push_back(std::move(vr_device_info));
  }

  return out_devices;
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

// These dispatchers must use Clone() instead of std::move to ensure that
// if there are multiple registered services they all get a copy of the data.
void VRDeviceManager::OnDeviceChanged(VRDisplayPtr device) {
  for (const auto& service : services_)
    service->client()->OnDisplayChanged(device.Clone());
}

void VRDeviceManager::InitializeProviders() {
  if (vr_initialized_) {
    return;
  }

  for (const auto& provider : providers_)
    provider->Initialize();

  vr_initialized_ = true;
}

void VRDeviceManager::RegisterProvider(
    std::unique_ptr<VRDeviceProvider> provider) {
  providers_.push_back(make_linked_ptr(provider.release()));
}

}  // namespace device
