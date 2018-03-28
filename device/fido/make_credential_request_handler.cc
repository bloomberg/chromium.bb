// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_device.h"
#include "device/fido/make_credential_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<U2fTransportProtocol>& protocols,
    CtapMakeCredentialRequest request_parameter,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    RegisterResponseCallback completion_callback)
    : FidoRequestHandler(connector, protocols),
      request_parameter_(std::move(request_parameter)),
      completion_callback_(std::move(completion_callback)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      weak_factory_(this) {}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

std::unique_ptr<FidoTask> MakeCredentialRequestHandler::CreateTaskForNewDevice(
    FidoDevice* device) {
  return std::make_unique<MakeCredentialTask>(
      device, request_parameter_, authenticator_selection_criteria_,
      base::BindOnce(&MakeCredentialRequestHandler::DispatchResponse,
                     weak_factory_.GetWeakPtr(), device));
}

void MakeCredentialRequestHandler::DispatchResponse(
    FidoDevice* device,
    CtapDeviceResponseCode return_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response_data) {
  if (is_complete()) {
    VLOG(2)
        << "Response from authenticator received after requests is complete.";
    return;
  }

  FidoReturnCode response_code = FidoReturnCode::kFailure;
  switch (return_code) {
    case CtapDeviceResponseCode::kSuccess:
      response_code =
          response_data ? FidoReturnCode::kSuccess : FidoReturnCode::kFailure;
      break;
    // These errors are only returned after the user interacted with the device.
    case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
    case CtapDeviceResponseCode::kCtap2ErrNotAllowed:
      response_code = FidoReturnCode::kConditionsNotSatisfied;
      break;
    default:
      // Silently cancel requests with processing error.
      ongoing_tasks().erase(device->GetId());
      return;
  }

  // Once response has been passed to the relying party, cancel all other on
  // going requests.
  set_is_complete();
  CancelOngoingTasks(device->GetId());
  std::move(completion_callback_).Run(response_code, std::move(response_data));
}

}  // namespace device
