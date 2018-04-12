// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/fido_device.h"
#include "device/fido/get_assertion_task.h"

namespace device {

GetAssertionRequestHandler::GetAssertionRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& protocols,
    CtapGetAssertionRequest request,
    SignResponseCallback completion_callback)
    : FidoRequestHandler(connector, protocols, std::move(completion_callback)),
      request_(std::move(request)),
      weak_factory_(this) {}

GetAssertionRequestHandler::~GetAssertionRequestHandler() = default;

std::unique_ptr<FidoTask> GetAssertionRequestHandler::CreateTaskForNewDevice(
    FidoDevice* device) {
  return std::make_unique<GetAssertionTask>(
      device, request_,
      base::BindOnce(&GetAssertionRequestHandler::OnDeviceResponse,
                     weak_factory_.GetWeakPtr(), device));
}

}  // namespace device
