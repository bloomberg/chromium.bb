// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multidevice/service/device_sync_service.h"

#include "components/multidevice/service/device_sync_impl.h"
#include "components/proximity_auth/logging/logging.h"

namespace device_sync {

DeviceSyncService::DeviceSyncService()
    : device_sync_impl_(std::make_unique<DeviceSyncImpl>()) {}

DeviceSyncService::~DeviceSyncService() = default;

void DeviceSyncService::OnStart() {
  PA_LOG(INFO) << "DeviceSyncService::OnStart()";
  registry_.AddInterface(base::Bind(&DeviceSyncImpl::BindRequest,
                                    base::Unretained(device_sync_impl_.get())));
}

void DeviceSyncService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  PA_LOG(INFO) << "DeviceSyncService::OnBindInterface() from interface "
               << interface_name << ".";
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace device_sync
