// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/strings/string_piece.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

FidoRequestHandlerBase::FidoRequestHandlerBase(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& transports) {
  for (const auto transport : transports) {
    auto discovery = FidoDiscovery::Create(transport, connector);
    if (discovery == nullptr) {
      // This can occur in tests when a ScopedVirtualU2fDevice is in effect and
      // HID transports are not configured.
      continue;
    }
    discovery->set_observer(this);
    discovery->Start();
    discoveries_.push_back(std::move(discovery));
  }
}

FidoRequestHandlerBase::~FidoRequestHandlerBase() = default;

void FidoRequestHandlerBase::CancelOngoingTasks(
    base::StringPiece exclude_device_id) {
  for (auto task_it = ongoing_tasks_.begin();
       task_it != ongoing_tasks_.end();) {
    DCHECK(!task_it->first.empty());
    if (task_it->first != exclude_device_id) {
      DCHECK(task_it->second);
      task_it->second->CancelTask();
      task_it = ongoing_tasks_.erase(task_it);
    } else {
      ++task_it;
    }
  }
}

void FidoRequestHandlerBase::DiscoveryStarted(FidoDiscovery* discovery,
                                              bool success) {}

void FidoRequestHandlerBase::DeviceAdded(FidoDiscovery* discovery,
                                         FidoDevice* device) {
  DCHECK(!base::ContainsKey(ongoing_tasks(), device->GetId()));
  // All devices are initially assumed to support CTAP protocol and thus
  // AuthenticatorGetInfo command is sent to all connected devices. If device
  // errors out, then it is assumed to support U2F protocol.
  device->set_supported_protocol(ProtocolVersion::kCtap);
  ongoing_tasks_.emplace(device->GetId(), CreateTaskForNewDevice(device));
}

void FidoRequestHandlerBase::DeviceRemoved(FidoDiscovery* discovery,
                                           FidoDevice* device) {
  // Device connection has been lost or device has already been removed.
  // Thus, calling CancelTask() is not necessary. Also, below
  // ongoing_tasks_.erase() will have no effect for the devices that have been
  // already removed due to processing error or due to invocation of
  // CancelOngoingTasks().
  ongoing_tasks_.erase(device->GetId());
}

}  // namespace device
