// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_

#include <memory>

#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace chromeos {

namespace device_sync {

class DeviceSyncImpl;

// Service which provides an implementation for
// device_sync::mojom::DeviceSync. This service creates one
// implementation and shares it among all connection requests.
class DeviceSyncService : public service_manager::Service {
 public:
  DeviceSyncService();
  ~DeviceSyncService() override;

 protected:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  std::unique_ptr<DeviceSyncImpl> device_sync_impl_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncService);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_SERVICE_H_
