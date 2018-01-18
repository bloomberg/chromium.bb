// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_sign.h"

#include <utility>

#include "device/u2f/u2f_discovery.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

U2fSign::U2fSign(std::string relying_party_id,
                 std::vector<U2fDiscovery*> discoveries,
                 const std::vector<std::vector<uint8_t>>& registered_keys,
                 const std::vector<uint8_t>& challenge_hash,
                 const std::vector<uint8_t>& app_param,
                 SignResponseCallback completion_callback)
    : U2fRequest(std::move(relying_party_id), std::move(discoveries)),
      registered_keys_(registered_keys),
      challenge_hash_(challenge_hash),
      app_param_(app_param),
      completion_callback_(std::move(completion_callback)),
      weak_factory_(this) {}

U2fSign::~U2fSign() = default;

// static
std::unique_ptr<U2fRequest> U2fSign::TrySign(
    std::string relying_party_id,
    std::vector<U2fDiscovery*> discoveries,
    const std::vector<std::vector<uint8_t>>& registered_keys,
    const std::vector<uint8_t>& challenge_hash,
    const std::vector<uint8_t>& app_param,
    SignResponseCallback completion_callback) {
  std::unique_ptr<U2fRequest> request = std::make_unique<U2fSign>(
      std::move(relying_party_id), std::move(discoveries), registered_keys,
      challenge_hash, app_param, std::move(completion_callback));
  request->Start();

  return request;
}

void U2fSign::TryDevice() {
  DCHECK(current_device_);

  if (registered_keys_.size() == 0) {
    // Send registration (Fake enroll) if no keys were provided
    current_device_->Register(
        U2fRequest::GetBogusAppParam(), U2fRequest::GetBogusChallenge(),
        base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                   registered_keys_.cend()));
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
    case U2fReturnCode::SUCCESS: {
      state_ = State::COMPLETE;
      if (it == registered_keys_.cend()) {
        // This was a response to a fake enrollment. Return an empty key handle.
        std::move(completion_callback_)
            .Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED, base::nullopt);
      } else {
        auto sign_response = SignResponseData::CreateFromU2fSignResponse(
            relying_party_id_, std::move(response_data), *it);
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
      if (++it != registered_keys_.end()) {
        // Key is not for this device. Try signing with the next key.
        current_device_->Sign(
            app_param_, challenge_hash_, *it,
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(), it));
      } else {
        // No provided key was accepted by this device. Send registration
        // (Fake enroll) request to device.
        current_device_->Register(
            U2fRequest::GetBogusAppParam(), U2fRequest::GetBogusChallenge(),
            base::Bind(&U2fSign::OnTryDevice, weak_factory_.GetWeakPtr(),
                       registered_keys_.cend()));
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
