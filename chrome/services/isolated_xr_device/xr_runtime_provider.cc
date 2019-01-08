// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_runtime_provider.h"
#include "chrome/common/chrome_features.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device.h"
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
#include "device/vr/oculus/oculus_device.h"
#endif

namespace {
// Poll for device add/remove every 5 seconds.
constexpr base::TimeDelta kTimeBetweenPollingEvents =
    base::TimeDelta::FromSecondsD(5);
}  // namespace

void IsolatedXRRuntimeProvider::PollForDeviceChanges(bool check_openvr,
                                                     bool check_oculus) {
#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (check_oculus) {
    bool oculus_available = (oculus_device_ && oculus_device_->IsAvailable()) ||
                            device::OculusDevice::IsHwAvailable();
    if (oculus_available && !oculus_device_) {
      oculus_device_ = std::make_unique<device::OculusDevice>();
      client_->OnDeviceAdded(oculus_device_->BindXRRuntimePtr(),
                             oculus_device_->BindGamepadFactory(),
                             oculus_device_->BindCompositorHost(),
                             oculus_device_->GetId());
    } else if (oculus_device_ && !oculus_available) {
      client_->OnDeviceRemoved(oculus_device_->GetId());
      oculus_device_ = nullptr;
    }
  }
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (check_openvr) {
    bool openvr_available =
        (openvr_device_ && !openvr_device_->IsAvailable()) ||
        device::OpenVRDevice::IsHwAvailable();
    if (openvr_available && !openvr_device_) {
      openvr_device_ = std::make_unique<device::OpenVRDevice>();
      client_->OnDeviceAdded(openvr_device_->BindXRRuntimePtr(),
                             openvr_device_->BindGamepadFactory(),
                             openvr_device_->BindCompositorHost(),
                             openvr_device_->GetId());
    } else if (openvr_device_ && !openvr_available) {
      client_->OnDeviceRemoved(openvr_device_->GetId());
      openvr_device_ = nullptr;
    }
  }
#endif

  if (check_openvr || check_oculus) {
    // Post a task to do this again later.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IsolatedXRRuntimeProvider::PollForDeviceChanges,
                       weak_ptr_factory_.GetWeakPtr(), check_openvr,
                       check_oculus),
        kTimeBetweenPollingEvents);
  }
}

void IsolatedXRRuntimeProvider::SetupPollingForDeviceChanges() {
  bool openvr_api_available = false;
  bool oculus_api_available = false;

#if BUILDFLAG(ENABLE_OCULUS_VR)
  if (base::FeatureList::IsEnabled(features::kOculusVR))
    oculus_api_available = device::OculusDevice::IsApiAvailable();
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  if (base::FeatureList::IsEnabled(features::kOpenVR))
    openvr_api_available = device::OpenVRDevice::IsApiAvailable();
#endif

  // Post a task to call back every periodically.
  if (openvr_api_available || oculus_api_available) {
    PollForDeviceChanges(openvr_api_available, oculus_api_available);
  }
}

void IsolatedXRRuntimeProvider::RequestDevices(
    device::mojom::IsolatedXRRuntimeProviderClientPtr client) {
  // Start polling to detect devices being added/removed.
  client_ = std::move(client);
  SetupPollingForDeviceChanges();
  client_->OnDevicesEnumerated();
}

IsolatedXRRuntimeProvider::IsolatedXRRuntimeProvider(
    std::unique_ptr<service_manager::ServiceKeepaliveRef> service_ref)
    : service_ref_(std::move(service_ref)), weak_ptr_factory_(this) {}

IsolatedXRRuntimeProvider::~IsolatedXRRuntimeProvider() {}
