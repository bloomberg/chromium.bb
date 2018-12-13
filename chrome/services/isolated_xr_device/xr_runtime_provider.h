// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_

#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace device {
class OculusDevice;
class OpenVRDevice;
}  // namespace device

class IsolatedXRRuntimeProvider
    : public device::mojom::IsolatedXRRuntimeProvider {
 public:
  IsolatedXRRuntimeProvider(
      std::unique_ptr<service_manager::ServiceKeepaliveRef> service_ref);
  ~IsolatedXRRuntimeProvider() final;

  void RequestDevices(
      device::mojom::IsolatedXRRuntimeProviderClientPtr client) override;

 private:
  const std::unique_ptr<service_manager::ServiceKeepaliveRef> service_ref_;

  IsolatedXRRuntimeProvider();
  void PollForDeviceChanges(bool check_openvr, bool check_oculus);
  void SetupPollingForDeviceChanges();

#if BUILDFLAG(ENABLE_OCULUS_VR)
  std::unique_ptr<device::OculusDevice> oculus_device_;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  std::unique_ptr<device::OpenVRDevice> openvr_device_;
#endif

  device::mojom::IsolatedXRRuntimeProviderClientPtr client_;
  base::WeakPtrFactory<IsolatedXRRuntimeProvider> weak_ptr_factory_;
};

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
