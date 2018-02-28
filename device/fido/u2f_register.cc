// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_register.h"

#include <utility>

#include "base/stl_util.h"
#include "device/fido/register_response_data.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

// static
std::unique_ptr<U2fRequest> U2fRegister::TryRegistration(
    service_manager::Connector* connector,
    const base::flat_set<U2fTransportProtocol>& protocols,
    std::vector<std::vector<uint8_t>> registered_keys,
    std::vector<uint8_t> challenge_digest,
    std::vector<uint8_t> application_parameter,
    bool individual_attestation_ok,
    RegisterResponseCallback completion_callback) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fRegister>(
      connector, protocols, std::move(registered_keys),
      std::move(challenge_digest), std::move(application_parameter),
      individual_attestation_ok, std::move(completion_callback));
  request->Start();
  return request;
}

U2fRegister::U2fRegister(service_manager::Connector* connector,
                         const base::flat_set<U2fTransportProtocol>& protocols,
                         std::vector<std::vector<uint8_t>> registered_keys,
                         std::vector<uint8_t> challenge_digest,
                         std::vector<uint8_t> application_parameter,
                         bool individual_attestation_ok,
                         RegisterResponseCallback completion_callback)
    : U2fRequest(connector,
                 protocols,
                 std::move(application_parameter),
                 std::move(challenge_digest),
                 std::move(registered_keys)),
      individual_attestation_ok_(individual_attestation_ok),
      completion_callback_(std::move(completion_callback)),
      weak_factory_(this) {}

U2fRegister::~U2fRegister() = default;

void U2fRegister::TryDevice() {
  DCHECK(current_device_);
  if (registered_keys_.size() > 0 && !CheckedForDuplicateRegistration()) {
    auto it = registered_keys_.cbegin();
    current_device_->Sign(application_parameter_, challenge_digest_, *it,
                          base::Bind(&U2fRegister::OnTryCheckRegistration,
                                     weak_factory_.GetWeakPtr(), it),
                          true);
  } else {
    current_device_->Register(application_parameter_, challenge_digest_,
                              individual_attestation_ok_,
                              base::Bind(&U2fRegister::OnTryDevice,
                                         weak_factory_.GetWeakPtr(), false));
  }
}

void U2fRegister::OnTryCheckRegistration(
    std::vector<std::vector<uint8_t>>::const_iterator it,
    U2fReturnCode return_code,
    const std::vector<uint8_t>& response_data) {
  switch (return_code) {
    case U2fReturnCode::SUCCESS:
    case U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // Duplicate registration found. Call bogus registration to check for
      // user presence (touch) and terminate the registration process.
      current_device_->Register(U2fRequest::GetBogusApplicationParameter(),
                                U2fRequest::GetBogusChallenge(),
                                false /* no individual attestation */,
                                base::Bind(&U2fRegister::OnTryDevice,
                                           weak_factory_.GetWeakPtr(), true));
      break;

    case U2fReturnCode::INVALID_PARAMS:
      // Continue to iterate through the provided key handles in the exclude
      // list and check for already registered keys.
      if (++it != registered_keys_.end()) {
        current_device_->Sign(application_parameter_, challenge_digest_, *it,
                              base::Bind(&U2fRegister::OnTryCheckRegistration,
                                         weak_factory_.GetWeakPtr(), it),
                              true);
      } else {
        checked_device_id_list_.insert(current_device_->GetId());
        if (devices_.empty()) {
          // When all devices have been checked, proceed to registration.
          CompleteNewDeviceRegistration();
        } else {
          state_ = State::IDLE;
          Transition();
        }
      }
      break;
    default:
      // Some sort of failure occurred. Abandon this device and move on.
      state_ = State::IDLE;
      current_device_ = nullptr;
      Transition();
      break;
  }
}

void U2fRegister::CompleteNewDeviceRegistration() {
  if (current_device_)
    attempted_devices_.push_back(std::move(current_device_));

  devices_.splice(devices_.end(), std::move(attempted_devices_));
  state_ = State::IDLE;
  Transition();
  return;
}

bool U2fRegister::CheckedForDuplicateRegistration() {
  return base::ContainsKey(checked_device_id_list_, current_device_->GetId());
}

void U2fRegister::OnTryDevice(bool is_duplicate_registration,
                              U2fReturnCode return_code,
                              const std::vector<uint8_t>& response_data) {
  switch (return_code) {
    case U2fReturnCode::SUCCESS: {
      state_ = State::COMPLETE;
      if (is_duplicate_registration) {
        std::move(completion_callback_)
            .Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED, base::nullopt);
        break;
      }
      auto response = RegisterResponseData::CreateFromU2fRegisterResponse(
          application_parameter_, std::move(response_data));
      if (!response) {
        // The response data was corrupted / didn't parse properly.
        std::move(completion_callback_)
            .Run(U2fReturnCode::FAILURE, base::nullopt);
        break;
      }
      std::move(completion_callback_)
          .Run(U2fReturnCode::SUCCESS, std::move(response));
      break;
    }
    case U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // Waiting for user touch, move on and try this device later.
      state_ = State::IDLE;
      Transition();
      break;
    default:
      state_ = State::IDLE;
      // An error has occurred, quit trying this device.
      current_device_ = nullptr;
      Transition();
      break;
  }
}

}  // namespace device
