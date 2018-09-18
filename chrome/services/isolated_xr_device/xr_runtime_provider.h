// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_

#include "device/vr/buildflags/buildflags.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace device {
class OculusDevice;
class OpenVRDevice;
}  // namespace device

class IsolatedXRRuntimeProvider
    : public device::mojom::IsolatedXRRuntimeProvider {
 public:
  IsolatedXRRuntimeProvider(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~IsolatedXRRuntimeProvider() final;

  void RequestDevices(
      device::mojom::IsolatedXRRuntimeProviderClientPtr client) override;

 private:
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;

  IsolatedXRRuntimeProvider();

#if BUILDFLAG(ENABLE_OCULUS_VR)
  std::unique_ptr<device::OculusDevice> oculus_device_;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  std::unique_ptr<device::OpenVRDevice> openvr_device_;
#endif

  device::mojom::IsolatedXRRuntimeProviderClientPtr client_;
};

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_RUNTIME_PROVIDER_H_
