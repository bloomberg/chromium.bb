// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

CredentialManagementHandler::CredentialManagementHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    Delegate* delegate)
    : FidoRequestHandlerBase(connector, supported_transports),
      delegate_(delegate),
      weak_factory_(this) {
  Start();
}

CredentialManagementHandler::~CredentialManagementHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CredentialManagementHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForTouch);
  authenticator->GetTouch(base::BindOnce(&CredentialManagementHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void CredentialManagementHandler::OnTouch(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForTouch) {
    return;
  }
  state_ = State::kGettingRetries;
  CancelActiveAuthenticators(authenticator->GetId());

  if (authenticator->SupportedProtocol() != ProtocolVersion::kCtap ||
      !authenticator->Options() ||
      !(authenticator->Options()->supports_credential_management ||
        authenticator->Options()->supports_credential_management_preview)) {
    state_ = State::kFinished;
    delegate_->OnError(
        FidoReturnCode::kAuthenticatorMissingCredentialManagement);
    return;
  }

  DCHECK(observer()->SupportsPIN());

  if (authenticator->Options()->client_pin_availability !=
      AuthenticatorSupportedOptions::ClientPinAvailability::
          kSupportedAndPinSet) {
    // The authenticator doesn't have a PIN/UV set up or doesn't support PINs.
    // We should implement in-flow PIN setting, but for now just tell the user
    // to set a PIN themselves.
    state_ = State::kFinished;
    delegate_->OnError(FidoReturnCode::kAuthenticatorMissingUserVerification);
    return;
  }

  authenticator_ = authenticator;
  authenticator_->GetRetries(
      base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);
  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    delegate_->OnError(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }
  if (response->retries == 0) {
    state_ = State::kFinished;
    delegate_->OnError(FidoReturnCode::kHardPINBlock);
    return;
  }
  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(response->retries,
                         base::BindOnce(&CredentialManagementHandler::OnHavePIN,
                                        weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForPIN, state_);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    return;
  }

  state_ = State::kGettingEphemeralKey;
  authenticator_->GetEphemeralKey(
      base::BindOnce(&CredentialManagementHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void CredentialManagementHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kGettingEphemeralKey, state_);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    delegate_->OnError(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }

  state_ = State::kGettingPINToken;
  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&CredentialManagementHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kGettingPINToken);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&CredentialManagementHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode error;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        error = FidoReturnCode::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        error = FidoReturnCode::kHardPINBlock;
        break;
      default:
        error = FidoReturnCode::kAuthenticatorResponseInvalid;
        break;
    }
    delegate_->OnError(error);
    return;
  }

  observer()->FinishCollectPIN();
  state_ = State::kGettingCredentials;

  authenticator_->GetCredentialsMetadata(
      response->token(),
      base::BindOnce(&CredentialManagementHandler::OnCredentialsMetadata,
                     weak_factory_.GetWeakPtr()));
}

void CredentialManagementHandler::OnCredentialsMetadata(
    CtapDeviceResponseCode status,
    base::Optional<CredentialsMetadataResponse> response) {
  if (status != CtapDeviceResponseCode::kSuccess) {
    delegate_->OnError(FidoReturnCode::kAuthenticatorResponseInvalid);
    return;
  }
  delegate_->OnCredentialMetadata(
      response->num_existing_credentials,
      response->num_estimated_remaining_credentials);
}

void CredentialManagementHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN) {
      state_ = State::kFinished;
      delegate_->OnError(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry);
    }
  }
}

}  // namespace device
