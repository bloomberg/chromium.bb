// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_sign.h"

#include <utility>

#include "device/u2f/u2f_discovery.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

U2fSign::U2fSign(const std::vector<std::vector<uint8_t>>& registered_keys,
                 const std::vector<uint8_t>& challenge_hash,
                 const std::vector<uint8_t>& app_param,
                 std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
                 const ResponseCallback& cb)
    : U2fRequest(std::move(discoveries), cb),
      registered_keys_(registered_keys),
      challenge_hash_(challenge_hash),
      app_param_(app_param),
      weak_factory_(this) {}

U2fSign::~U2fSign() {}

// static
std::unique_ptr<U2fRequest> U2fSign::TrySign(
    const std::vector<std::vector<uint8_t>>& registered_keys,
    const std::vector<uint8_t>& challenge_hash,
    const std::vector<uint8_t>& app_param,
    std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
    const ResponseCallback& cb) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fSign>(
      registered_keys, challenge_hash, app_param, std::move(discoveries), cb);
  request->Start();

  return request;
}

void U2fSign::TryDevice() {
  DCHECK(current_device_);

  if (registered_keys_.size() == 0) {
    // Send registration (Fake enroll) if no keys were provided
    current_device_->Register(
        kBogusAppParam, kBogusChallenge,
        base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                   registered_keys_.cbegin()));
    return;
  }
  // Try signing current device with the first registered key
  auto it = registered_keys_.cbegin();
  current_device_->Sign(
      app_param_, challenge_hash_, *it,
      base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it));
}

void U2fSign::OnTryDevice(std::vector<std::vector<uint8_t>>::const_iterator it,
                          U2fReturnCode return_code,
                          const std::vector<uint8_t>& response_data) {
  switch (return_code) {
    case U2fReturnCode::SUCCESS:
      state_ = State::COMPLETE;
      cb_.Run(return_code, response_data);
      break;
    case U2fReturnCode::CONDITIONS_NOT_SATISFIED: {
      // Key handle is accepted by this device, but waiting on user touch. Move
      // on and try this device again later.
      state_ = State::IDLE;
      Transition();
      break;
    }
    case U2fReturnCode::INVALID_PARAMS:
      if (++it != registered_keys_.end()) {
        // Key is not for this device. Try signing with the next key.
        current_device_->Sign(
            app_param_, challenge_hash_, *it,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it));
      } else {
        // No provided key was accepted by this device. Send registration
        // (Fake enroll) request to device.
        current_device_->Register(
            kBogusAppParam, kBogusChallenge,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                       registered_keys_.cbegin()));
      }
      break;
    default:
      // Some sort of failure occured. Abandon this device and move on.
      state_ = State::IDLE;
      current_device_ = nullptr;
      Transition();
      break;
  }
}

}  // namespace device
