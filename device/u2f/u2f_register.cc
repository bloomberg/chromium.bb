// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_register.h"

#include <utility>

#include "base/stl_util.h"
#include "device/u2f/u2f_discovery.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

U2fRegister::U2fRegister(
    const std::vector<std::vector<uint8_t>>& registered_keys,
    const std::vector<uint8_t>& challenge_hash,
    const std::vector<uint8_t>& app_param,
    std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
    const ResponseCallback& cb)
    : U2fRequest(std::move(discoveries), cb),
      challenge_hash_(challenge_hash),
      app_param_(app_param),
      registered_keys_(registered_keys),
      weak_factory_(this) {}

U2fRegister::~U2fRegister() = default;

// static
std::unique_ptr<U2fRequest> U2fRegister::TryRegistration(
    const std::vector<std::vector<uint8_t>>& registered_keys,
    const std::vector<uint8_t>& challenge_hash,
    const std::vector<uint8_t>& app_param,
    std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
    const ResponseCallback& cb) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fRegister>(
      registered_keys, challenge_hash, app_param, std::move(discoveries), cb);
  request->Start();
  return request;
}

void U2fRegister::TryDevice() {
  DCHECK(current_device_);
  if (registered_keys_.size() > 0 && !CheckedForDuplicateRegistration()) {
    auto it = registered_keys_.cbegin();
    current_device_->Sign(app_param_, challenge_hash_, *it,
                          base::Bind(&U2fRegister::OnTryCheckRegistration,
                                     weak_factory_.GetWeakPtr(), it),
                          true);
  } else {
    current_device_->Register(app_param_, challenge_hash_,
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
      current_device_->Register(U2fRequest::GetBogusAppParam(),
                                U2fRequest::GetBogusChallenge(),
                                base::Bind(&U2fRegister::OnTryDevice,
                                           weak_factory_.GetWeakPtr(), true));
      break;

    case U2fReturnCode::INVALID_PARAMS:
      // Continue to iterate through the provided key handles in the exclude
      // list and check for already registered keys.
      if (++it != registered_keys_.end()) {
        current_device_->Sign(app_param_, challenge_hash_, *it,
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
    case U2fReturnCode::SUCCESS:
      state_ = State::COMPLETE;
      if (is_duplicate_registration)
        return_code = U2fReturnCode::CONDITIONS_NOT_SATISFIED;
      cb_.Run(return_code, response_data, std::vector<uint8_t>());
      break;
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
