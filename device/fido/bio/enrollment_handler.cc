// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment_handler.h"

#include "base/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"

namespace device {

BioEnrollmentHandler::BioEnrollmentHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    ReadyCallback ready_callback,
    GetPINCallback get_pin_callback,
    FidoDiscoveryFactory* factory)
    : FidoRequestHandlerBase(connector, factory, supported_transports),
      ready_callback_(std::move(ready_callback)),
      get_pin_callback_(std::move(get_pin_callback)),
      weak_factory_(this) {
  Start();
}

BioEnrollmentHandler::~BioEnrollmentHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
}

void BioEnrollmentHandler::GetModality(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  if (!authenticator_ ||
      authenticator_->Options()->bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
                            base::nullopt);
  } else {
    authenticator_->GetModality(std::move(callback));
  }
}

void BioEnrollmentHandler::GetSensorInfo(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  if (!authenticator_ ||
      authenticator_->Options()->bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrUnsupportedOption,
                            base::nullopt);
  } else {
    authenticator_->GetSensorInfo(std::move(callback));
  }
}

void BioEnrollmentHandler::Enroll(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  response_callback_ = std::move(callback);
  authenticator_->BioEnrollFingerprint(
      *pin_token_response_, base::BindRepeating(&BioEnrollmentHandler::OnEnroll,
                                                weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::Cancel(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);

  status_callback_ = std::move(callback);
  authenticator_->BioEnrollCancel(base::BindOnce(
      &BioEnrollmentHandler::OnCancel, weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::DispatchRequest(FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
  authenticator->GetTouch(base::BindOnce(&BioEnrollmentHandler::OnTouch,
                                         weak_factory_.GetWeakPtr(),
                                         authenticator));
}

void BioEnrollmentHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  if (authenticator_ == authenticator) {
    authenticator_ = nullptr;
  }
  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);
}

void BioEnrollmentHandler::OnTouch(FidoAuthenticator* authenticator) {
  CancelActiveAuthenticators(authenticator->GetId());

  if (!authenticator->Options() ||
      authenticator->Options()->bio_enrollment_availability_preview ==
          AuthenticatorSupportedOptions::BioEnrollmentAvailability::
              kNotSupported) {
    std::move(ready_callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther);
    return;
  }

  authenticator_ = authenticator;
  authenticator_->GetRetries(base::BindOnce(
      &BioEnrollmentHandler::OnRetriesResponse, weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnRetriesResponse(
    CtapDeviceResponseCode code,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
  if (code != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(DEBUG) << "OnRetriesResponse failed: " << static_cast<int>(code);
    std::move(ready_callback_).Run(code);
    return;
  }

  get_pin_callback_.Run(response->retries,
                        base::BindOnce(&BioEnrollmentHandler::OnHavePIN,
                                       weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePIN(std::string pin) {
  authenticator_->GetEphemeralKey(
      base::BindOnce(&BioEnrollmentHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void BioEnrollmentHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode code,
    base::Optional<pin::KeyAgreementResponse> response) {
  if (code != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(DEBUG) << "OnHaveEphemeralKey failed: " << static_cast<int>(code);
    std::move(ready_callback_).Run(code);
    return;
  }
  authenticator_->GetPINToken(
      std::move(pin), *response,
      base::BindOnce(&BioEnrollmentHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void BioEnrollmentHandler::OnHavePINToken(
    CtapDeviceResponseCode code,
    base::Optional<pin::TokenResponse> response) {
  if (code == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    authenticator_->GetRetries(base::BindOnce(
        &BioEnrollmentHandler::OnRetriesResponse, weak_factory_.GetWeakPtr()));
    return;
  }

  if (!response || code != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(DEBUG) << "OnHavePINToken failed: " << static_cast<int>(code);
    std::move(ready_callback_).Run(code);
    return;
  }

  pin_token_response_ = *response;

  std::move(ready_callback_).Run(CtapDeviceResponseCode::kSuccess);
}

void BioEnrollmentHandler::OnEnroll(
    CtapDeviceResponseCode code,
    base::Optional<BioEnrollmentResponse> response) {
  FIDO_LOG(DEBUG) << "Finished bio enrollment with code "
                  << static_cast<int>(code);
  std::move(response_callback_).Run(code, std::move(response));
}

void BioEnrollmentHandler::OnCancel(CtapDeviceResponseCode,
                                    base::Optional<BioEnrollmentResponse>) {
  std::move(status_callback_).Run();
}

}  // namespace device
