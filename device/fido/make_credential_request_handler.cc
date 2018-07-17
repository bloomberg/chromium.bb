// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/make_credential_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

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
  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

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

}  // namespace

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
      base::BindOnce(&MakeCredentialRequestHandler::OnAuthenticatorResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

}  // namespace device
