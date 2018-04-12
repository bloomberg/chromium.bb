// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_device.h"
#include "device/fido/make_credential_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& protocols,
    CtapMakeCredentialRequest request_parameter,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    RegisterResponseCallback completion_callback)
    : FidoRequestHandler(connector, protocols, std::move(completion_callback)),
      request_parameter_(std::move(request_parameter)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      weak_factory_(this) {}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

std::unique_ptr<FidoTask> MakeCredentialRequestHandler::CreateTaskForNewDevice(
    FidoDevice* device) {
  return std::make_unique<MakeCredentialTask>(
      device, request_parameter_, authenticator_selection_criteria_,
      base::BindOnce(&MakeCredentialRequestHandler::OnDeviceResponse,
                     weak_factory_.GetWeakPtr(), device));
}

}  // namespace device
