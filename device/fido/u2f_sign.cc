// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign.h"

#include <utility>

#include "components/apdu/apdu_command.h"
#include "components/apdu/apdu_response.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

// static
std::unique_ptr<U2fRequest> U2fSign::TrySign(
    service_manager::Connector* connector,
    const base::flat_set<U2fTransportProtocol>& transports,
    std::vector<std::vector<uint8_t>> registered_keys,
    std::vector<uint8_t> challenge_digest,
    std::vector<uint8_t> application_parameter,
    base::Optional<std::vector<uint8_t>> alt_application_parameter,
    SignResponseCallback completion_callback) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fSign>(
      connector, transports, registered_keys, challenge_digest,
      application_parameter, std::move(alt_application_parameter),
      std::move(completion_callback));
  request->Start();

  return request;
}

U2fSign::U2fSign(service_manager::Connector* connector,
                 const base::flat_set<U2fTransportProtocol>& transports,
                 std::vector<std::vector<uint8_t>> registered_keys,
                 std::vector<uint8_t> challenge_digest,
                 std::vector<uint8_t> application_parameter,
                 base::Optional<std::vector<uint8_t>> alt_application_parameter,
                 SignResponseCallback completion_callback)
    : U2fRequest(connector,
                 transports,
                 std::move(application_parameter),
                 std::move(challenge_digest),
                 std::move(registered_keys)),
      alt_application_parameter_(std::move(alt_application_parameter)),
      completion_callback_(std::move(completion_callback)),
      weak_factory_(this) {}

U2fSign::~U2fSign() = default;

void U2fSign::TryDevice() {
  DCHECK(current_device_);

  // There are two different descriptions of what should happen when
  // "allowCredentials" is empty.
  // a) WebAuthN 6.2.3 step 6[1] implies "NotAllowedError". The current
  // implementation returns this in response to receiving
  // CONDITIONS_NOT_SATISFIED from TrySign.
  // b) CTAP step 7.2 step 2[2] says the device should error out with
  // "CTAP2_ERR_OPTION_NOT_SUPPORTED". This also resolves to "NotAllowedError".
  // The behavior in both cases is consistent with the current implementation.
  // When CTAP2 authenticators are supported, this check should be enforced by
  // handlers in fido/device on a per-device basis.

  // [1] https://w3c.github.io/webauthn/#authenticatorgetassertion
  // [2]
  // https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html
  if (registered_keys_.size() == 0) {
    // Send registration (Fake enroll) if no keys were provided.
    InitiateDeviceTransaction(
        U2fRequest::GetBogusRegisterCommand(),
        base::BindOnce(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                       registered_keys_.cend(),
                       ApplicationParameterType::kPrimary));
    return;
  }
  // Try signing current device with the first registered key.
  auto it = registered_keys_.cbegin();
  InitiateDeviceTransaction(
      GetU2fSignApduCommand(application_parameter_, *it),
      base::BindOnce(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it,
                     ApplicationParameterType::kPrimary));
}

void U2fSign::OnTryDevice(std::vector<std::vector<uint8_t>>::const_iterator it,
                          ApplicationParameterType application_parameter_type,
                          base::Optional<std::vector<uint8_t>> response) {
  const auto apdu_response =
      response ? apdu::ApduResponse::CreateFromMessage(std::move(*response))
               : base::nullopt;
  auto return_code = apdu_response ? apdu_response->status()
                                   : apdu::ApduResponse::Status::SW_WRONG_DATA;
  auto response_data = return_code == apdu::ApduResponse::Status::SW_WRONG_DATA
                           ? std::vector<uint8_t>()
                           : apdu_response->data();

  switch (return_code) {
    case apdu::ApduResponse::Status::SW_NO_ERROR: {
      state_ = State::COMPLETE;
      if (it == registered_keys_.cend()) {
        // This was a response to a fake enrollment. Return an empty key handle.
        std::move(completion_callback_)
            .Run(FidoReturnCode::kConditionsNotSatisfied, base::nullopt);
      } else {
        const std::vector<uint8_t>* const application_parameter_used =
            application_parameter_type == ApplicationParameterType::kPrimary
                ? &application_parameter_
                : &alt_application_parameter_.value();
        auto sign_response =
            AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
                *application_parameter_used, std::move(response_data), *it);
        if (!sign_response) {
          std::move(completion_callback_)
              .Run(FidoReturnCode::kFailure, base::nullopt);
        } else {
          std::move(completion_callback_)
              .Run(FidoReturnCode::kSuccess, std::move(sign_response));
        }
      }
      break;
    }
    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED: {
      // Key handle is accepted by this device, but waiting on user touch. Move
      // on and try this device again later.
      state_ = State::IDLE;
      Transition();
      break;
    }
    case apdu::ApduResponse::Status::SW_WRONG_DATA:
    case apdu::ApduResponse::Status::SW_WRONG_LENGTH: {
      if (application_parameter_type == ApplicationParameterType::kPrimary &&
          alt_application_parameter_) {
        // |application_parameter_| failed, but there is also
        // |alt_application_parameter_| to try.
        InitiateDeviceTransaction(
            GetU2fSignApduCommand(*alt_application_parameter_, *it),
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it,
                       ApplicationParameterType::kAlternative));
      } else if (it == registered_keys_.cend()) {
        // The fake enrollment errored out. Move on to the next device.
        state_ = State::IDLE;
        current_device_ = nullptr;
        Transition();
      } else if (++it != registered_keys_.end()) {
        // Key is not for this device. Try signing with the next key.
        InitiateDeviceTransaction(
            GetU2fSignApduCommand(application_parameter_, *it),
            base::BindOnce(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                           it, ApplicationParameterType::kPrimary));
      } else {
        // No provided key was accepted by this device. Send registration
        // (Fake enroll) request to device.
        InitiateDeviceTransaction(
            U2fRequest::GetBogusRegisterCommand(),
            base::BindOnce(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                           registered_keys_.cend(),
                           ApplicationParameterType::kPrimary));
      }
      break;
    }
    default:
      // Some sort of failure occured. Abandon this device and move on.
      state_ = State::IDLE;
      current_device_ = nullptr;
      Transition();
      break;
  }
}

}  // namespace device
