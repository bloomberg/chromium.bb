// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

namespace {

bool CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria,
    CtapMakeCredentialRequest* request) {
  using AuthenticatorAttachment =
      AuthenticatorSelectionCriteria::AuthenticatorAttachment;
  using UvAvailability =
      AuthenticatorSupportedOptions::UserVerificationAvailability;

  const auto& options = authenticator->Options();
  if ((authenticator_selection_criteria.authenticator_attachement() ==
           AuthenticatorAttachment::kPlatform &&
       !options.is_platform_device()) ||
      (authenticator_selection_criteria.authenticator_attachement() ==
           AuthenticatorAttachment::kCrossPlatform &&
       options.is_platform_device())) {
    return false;
  }

  if (authenticator_selection_criteria.require_resident_key() &&
      !options.supports_resident_key()) {
    return false;
  }
  request->SetResidentKeySupported(
      authenticator_selection_criteria.require_resident_key());

  const auto& user_verification_requirement =
      authenticator_selection_criteria.user_verification_requirement();
  if (user_verification_requirement == UserVerificationRequirement::kRequired) {
    request->SetUserVerificationRequired(true);
  }

  return user_verification_requirement !=
             UserVerificationRequirement::kRequired ||
         options.user_verification_availability() ==
             UvAvailability::kSupportedAndConfigured;
}

std::set<FidoTransportProtocol> GetValidTransportProtocols(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  using AttachmentType =
      AuthenticatorSelectionCriteria::AuthenticatorAttachment;
  const auto attachment_type =
      authenticator_selection_criteria.authenticator_attachement();
  switch (attachment_type) {
    case AttachmentType::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AttachmentType::kCrossPlatform:
      return {FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
    case AttachmentType::kAny:
      return {FidoTransportProtocol::kInternal,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
  }

  NOTREACHED();
  return std::set<FidoTransportProtocol>();
}

}  // namespace

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& protocols,
    CtapMakeCredentialRequest request,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    RegisterResponseCallback completion_callback)
    : MakeCredentialRequestHandler(connector,
                                   protocols,
                                   std::move(request),
                                   authenticator_selection_criteria,
                                   std::move(completion_callback),
                                   AddPlatformAuthenticatorCallback()) {}

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& protocols,
    CtapMakeCredentialRequest request,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    RegisterResponseCallback completion_callback,
    AddPlatformAuthenticatorCallback add_platform_authenticator)
    : FidoRequestHandler(connector,
                         protocols,
                         std::move(completion_callback),
                         std::move(add_platform_authenticator)),
      request_parameter_(std::move(request)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      weak_factory_(this) {
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;
  transport_availability_info().available_transports =
      GetValidTransportProtocols(authenticator_selection_criteria);
  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  // The user verification field of the request may be adjusted to the
  // authenticator, so we need to make a copy.
  CtapMakeCredentialRequest request_copy = request_parameter_;
  if (!CheckIfAuthenticatorSelectionCriteriaAreSatisfied(
          authenticator, authenticator_selection_criteria_, &request_copy)) {
    return;
  }

  authenticator->MakeCredential(
      std::move(request_copy),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    OnAuthenticatorResponse(authenticator, response_code, base::nullopt);
    return;
  }

  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(request_parameter_.rp().rp_id());

  if (!response || response->GetRpIdHash() != rp_id_hash) {
    OnAuthenticatorResponse(
        authenticator, CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  OnAuthenticatorResponse(authenticator, response_code, std::move(response));
}

}  // namespace device
