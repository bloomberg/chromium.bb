// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler.h"

#include <utility>

#include "base/strings/string_piece.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

FidoRequestHandler::FidoRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<U2fTransportProtocol>& transports) {
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

FidoRequestHandler::~FidoRequestHandler() = default;

void FidoRequestHandler::CancelOngoingTasks(
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

void FidoRequestHandler::DiscoveryStarted(FidoDiscovery* discovery,
                                          bool success) {}

void FidoRequestHandler::DeviceAdded(FidoDiscovery* discovery,
                                     FidoDevice* device) {
  DCHECK(!base::ContainsKey(ongoing_tasks(), device->GetId()));
  ongoing_tasks_.emplace(device->GetId(), CreateTaskForNewDevice(device));
}

void FidoRequestHandler::DeviceRemoved(FidoDiscovery* discovery,
                                       FidoDevice* device) {
  // Device connection has been lost or device has already been removed.
  // Thus, calling CancelTask() is not necessary.
  DCHECK(base::ContainsKey(ongoing_tasks_, device->GetId()));
  ongoing_tasks_.erase(device->GetId());
}

}  // namespace device
