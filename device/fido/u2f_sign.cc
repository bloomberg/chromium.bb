// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign.h"

#include <utility>

#include "services/service_manager/public/cpp/connector.h"

namespace device {

// static
std::unique_ptr<U2fRequest> U2fSign::TrySign(
    service_manager::Connector* connector,
    const base::flat_set<U2fTransportProtocol>& protocols,
    std::vector<std::vector<uint8_t>> registered_keys,
    std::vector<uint8_t> challenge_digest,
    std::vector<uint8_t> application_parameter,
    base::Optional<std::vector<uint8_t>> alt_application_parameter,
    SignResponseCallback completion_callback) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fSign>(
      connector, protocols, registered_keys, challenge_digest,
      application_parameter, std::move(alt_application_parameter),
      std::move(completion_callback));
  request->Start();

  return request;
}

U2fSign::U2fSign(service_manager::Connector* connector,
                 const base::flat_set<U2fTransportProtocol>& protocols,
                 std::vector<std::vector<uint8_t>> registered_keys,
                 std::vector<uint8_t> challenge_digest,
                 std::vector<uint8_t> application_parameter,
                 base::Optional<std::vector<uint8_t>> alt_application_parameter,
                 SignResponseCallback completion_callback)
    : U2fRequest(connector,
                 protocols,
                 std::move(application_parameter),
                 std::move(challenge_digest),
                 std::move(registered_keys)),
      alt_application_parameter_(std::move(alt_application_parameter)),
      completion_callback_(std::move(completion_callback)),
      weak_factory_(this) {}

U2fSign::~U2fSign() = default;

void U2fSign::TryDevice() {
  DCHECK(current_device_);

  if (registered_keys_.size() == 0) {
    // Send registration (Fake enroll) if no keys were provided
    current_device_->Register(
        U2fRequest::GetBogusApplicationParameter(),
        U2fRequest::GetBogusChallenge(), false /* no individual attestation */,
        base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                   registered_keys_.cend(),
                   ApplicationParameterType::kPrimary));
    return;
  }
  // Try signing current device with the first registered key
  auto it = registered_keys_.cbegin();
  current_device_->Sign(
      application_parameter_, challenge_digest_, *it,
      base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it,
                 ApplicationParameterType::kPrimary));
}

void U2fSign::OnTryDevice(std::vector<std::vector<uint8_t>>::const_iterator it,
                          ApplicationParameterType application_parameter_type,
                          U2fReturnCode return_code,
                          const std::vector<uint8_t>& response_data) {
  switch (return_code) {
    case U2fReturnCode::SUCCESS: {
      state_ = State::COMPLETE;
      if (it == registered_keys_.cend()) {
        // This was a response to a fake enrollment. Return an empty key handle.
        std::move(completion_callback_)
            .Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED, base::nullopt);
      } else {
        const std::vector<uint8_t>* const application_parameter_used =
            application_parameter_type == ApplicationParameterType::kPrimary
                ? &application_parameter_
                : &alt_application_parameter_.value();
        auto sign_response = SignResponseData::CreateFromU2fSignResponse(
            *application_parameter_used, std::move(response_data), *it);
        if (!sign_response) {
          std::move(completion_callback_)
              .Run(U2fReturnCode::FAILURE, base::nullopt);
        } else {
          std::move(completion_callback_)
              .Run(U2fReturnCode::SUCCESS, std::move(sign_response));
        }
      }
      break;
    }
    case U2fReturnCode::CONDITIONS_NOT_SATISFIED: {
      // Key handle is accepted by this device, but waiting on user touch. Move
      // on and try this device again later.
      state_ = State::IDLE;
      Transition();
      break;
    }
    case U2fReturnCode::INVALID_PARAMS: {
      if (application_parameter_type == ApplicationParameterType::kPrimary &&
          alt_application_parameter_) {
        // |application_parameter_| failed, but there is also
        // |alt_application_parameter_| to try.
        current_device_->Sign(
            *alt_application_parameter_, challenge_digest_, *it,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it,
                       ApplicationParameterType::kAlternative));
      } else if (++it != registered_keys_.end()) {
        // Key is not for this device. Try signing with the next key.
        current_device_->Sign(
            application_parameter_, challenge_digest_, *it,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it,
                       ApplicationParameterType::kPrimary));
      } else {
        // No provided key was accepted by this device. Send registration
        // (Fake enroll) request to device.
        current_device_->Register(
            U2fRequest::GetBogusApplicationParameter(),
            U2fRequest::GetBogusChallenge(),
            false /* no individual attestation */,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
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
