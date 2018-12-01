// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace device {

class XrDeviceService : public service_manager::Service {
 public:
  static std::unique_ptr<service_manager::Service> CreateXrDeviceService();

  XrDeviceService();
  ~XrDeviceService() override;
  void OnDeviceProviderRequest(
      device::mojom::IsolatedXRRuntimeProviderRequest request);
  void OnTestHookRequest(
      device_test::mojom::XRTestHookRegistrationRequest request);

 private:
  // service_manager::Service
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::BinderRegistry registry_;

  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(XrDeviceService);
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
