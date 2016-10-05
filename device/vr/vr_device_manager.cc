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
    : vr_initialized_(false),
      presenting_service_(nullptr),
      presenting_device_(nullptr),
      keep_alive_(false),
      has_scheduled_poll_(false) {
// Register VRDeviceProviders for the current platform
#if defined(OS_ANDROID)
  RegisterProvider(base::WrapUnique(new GvrDeviceProvider()));
#endif
}

VRDeviceManager::VRDeviceManager(std::unique_ptr<VRDeviceProvider> provider)
    : vr_initialized_(false),
      presenting_service_(nullptr),
      presenting_device_(nullptr),
      keep_alive_(true),
      has_scheduled_poll_(false) {
  thread_checker_.DetachFromThread();
  RegisterProvider(std::move(provider));
  SetInstance(this);
}

VRDeviceManager::~VRDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StopSchedulingPollEvents();
  g_vr_device_manager = nullptr;
}

VRDeviceManager* VRDeviceManager::GetInstance() {
  if (!g_vr_device_manager)
    g_vr_device_manager = new VRDeviceManager();
  return g_vr_device_manager;
}

// Returns the requested device with the requested id if the specified service
// is allowed to access it.
VRDevice* VRDeviceManager::GetAllowedDevice(VRServiceImpl* service,
                                            unsigned int index) {
  VRDeviceManager* device_manager = GetInstance();

  // If another service is presenting to the requested device don't allow other
  // services to access it. That could potentially allow them to spy on
  // where the user is looking on another page, spam another application with
  // pose resets, etc.
  if (device_manager->presenting_service_ &&
      device_manager->presenting_service_ != service) {
    if (device_manager->presenting_device_ &&
        device_manager->presenting_device_->id() == index)
      return nullptr;
  }

  return device_manager->GetDevice(index);
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

  if (service == presenting_service_) {
    presenting_device_->ExitPresent();

    presenting_service_ = nullptr;
    presenting_device_ = nullptr;
  }

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

    // GetVRDevice should always set the index of the VRDisplay to its own id.
    DCHECK(vr_device_info->index == device->id());

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

bool VRDeviceManager::RequestPresent(VRServiceImpl* service,
                                     unsigned int index,
                                     bool secure_origin) {
  // Is anything presenting currently?
  if (presenting_service_) {
    // Should never have a presenting service without a presenting device.
    DCHECK(presenting_device_);

    // Fail if the currently presenting service is not the one making the
    // request.
    if (presenting_service_ != service)
      return false;

    // If we are switching presentation from the currently presenting service to
    // a new device stop presening to the previous one.
    if (presenting_device_->id() != index) {
      // Tell the device to stop presenting.
      presenting_device_->ExitPresent();

      // Only the presenting service needs to be notified that presentation is
      // ending on the previous device.
      presenting_service_->client()->OnExitPresent(presenting_device_->id());
      presenting_device_ = nullptr;
    }

    presenting_service_ = nullptr;
  }

  VRDevice* requested_device = GetDevice(index);
  // Can't present to a device that doesn't exist.
  if (!requested_device)
    return false;

  // Attempt to begin presenting to this device. This could fail for any number
  // of device-specific reasons.
  if (!requested_device->RequestPresent(secure_origin))
    return false;

  // Successfully began presenting!
  presenting_service_ = service;
  presenting_device_ = requested_device;

  return true;
}

void VRDeviceManager::ExitPresent(VRServiceImpl* service, unsigned int index) {
  // Don't allow services other than the currently presenting one to exit
  // presentation.
  if (presenting_service_ != service)
    return;

  // Should never have a presenting service without a presenting device.
  DCHECK(presenting_device_);

  // Fail if the specified device is not currently presenting.
  if (presenting_device_->id() != index)
    return;

  // Tell the client we're done. This must happen before the device's
  // ExitPresent to avoid invalid mojo state.
  if (presenting_service_->client()) {
    presenting_service_->client()->OnExitPresent(index);
  }

  // Tell the device to stop presenting.
  presenting_device_->ExitPresent();

  // Clear the presenting service and device.
  presenting_service_ = nullptr;
  presenting_device_ = nullptr;
}

void VRDeviceManager::SubmitFrame(VRServiceImpl* service,
                                  unsigned int index,
                                  VRPosePtr pose) {
  // Don't allow services other than the currently presenting one to submit any
  // frames.
  if (presenting_service_ != service)
    return;

  // Should never have a presenting service without a presenting device.
  DCHECK(presenting_device_);

  // Don't submit frames to devices other than the currently presenting one.
  if (presenting_device_->id() != index)
    return;

  presenting_device_->SubmitFrame(std::move(pose));
}

void VRDeviceManager::OnDeviceConnectionStatusChanged(VRDevice* device,
                                                      bool is_connected) {
  if (is_connected) {
    VRDisplayPtr vr_device_info = device->GetVRDevice();
    if (vr_device_info.is_null())
      return;

    vr_device_info->index = device->id();

    for (const auto& service : services_)
      service->client()->OnDisplayConnected(vr_device_info.Clone());
  } else {
    for (const auto& service : services_)
      service->client()->OnDisplayDisconnected(device->id());
  }
}

void VRDeviceManager::OnPresentEnded(VRDevice* device) {
  // Ensure the presenting device is the one that we've been requested to stop.
  if (!presenting_device_ || presenting_device_ != device)
    return;

  // Should never have a presenting device without a presenting service.
  DCHECK(presenting_service_);

  // Notify the presenting service that it's been forced to end presentation.
  presenting_service_->client()->OnExitPresent(device->id());

  // Clear the presenting service and device.
  presenting_service_ = nullptr;
  presenting_device_ = nullptr;
}

void VRDeviceManager::InitializeProviders() {
  if (vr_initialized_) {
    return;
  }

  for (const auto& provider : providers_) {
    provider->SetClient(this);
    provider->Initialize();
  }

  vr_initialized_ = true;
}

void VRDeviceManager::RegisterProvider(
    std::unique_ptr<VRDeviceProvider> provider) {
  providers_.push_back(make_linked_ptr(provider.release()));
}

void VRDeviceManager::SchedulePollEvents() {
  if (has_scheduled_poll_)
    return;

  has_scheduled_poll_ = true;

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(500), this,
               &VRDeviceManager::PollEvents);
}

void VRDeviceManager::PollEvents() {
  for (const auto& provider : providers_)
    provider->PollEvents();
}

void VRDeviceManager::StopSchedulingPollEvents() {
  if (has_scheduled_poll_)
    timer_.Stop();
}

}  // namespace device
