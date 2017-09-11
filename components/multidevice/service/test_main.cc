// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multidevice/service/device_sync_impl.h"
#include "components/multidevice/service/fake_device_sync.h"
#include "components/multidevice/service/multidevice_service.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"

namespace {

class FakeDeviceSyncFactory : public multidevice::DeviceSyncImpl::Factory {
 public:
  FakeDeviceSyncFactory() {}
  ~FakeDeviceSyncFactory() override {}

  // DeviceSyncImpl::Factory:
  std::unique_ptr<device_sync::mojom::DeviceSync> BuildInstance(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref)
      override {
    return base::MakeUnique<multidevice::FakeDeviceSync>();
  }
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  multidevice::DeviceSyncImpl::Factory::SetInstanceForTesting(
      new FakeDeviceSyncFactory());
  service_manager::ServiceRunner runner(new multidevice::MultiDeviceService());
  return runner.Run(service_request_handle);
}
