// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_register.h"

#include "base/memory/ptr_util.h"

namespace device {

U2fRegister::U2fRegister(const std::vector<uint8_t>& challenge_hash,
                         const std::vector<uint8_t>& app_param,
                         const ResponseCallback& cb)
    : U2fRequest(cb),
      challenge_hash_(challenge_hash),
      app_param_(app_param),
      weak_factory_(this) {}

U2fRegister::~U2fRegister() {}

// static
std::unique_ptr<U2fRequest> U2fRegister::TryRegistration(
    const std::vector<uint8_t>& challenge_hash,
    const std::vector<uint8_t>& app_param,
    const ResponseCallback& cb) {
  std::unique_ptr<U2fRequest> request =
      base::MakeUnique<U2fRegister>(challenge_hash, app_param, cb);
  request->Start();
  return request;
}

void U2fRegister::TryDevice() {
  DCHECK(current_device_);

  current_device_->Register(
      app_param_, challenge_hash_,
      base::Bind(&U2fRegister::OnTryDevice, weak_factory_.GetWeakPtr()));
}

void U2fRegister::OnTryDevice(U2fReturnCode return_code,
                              std::vector<uint8_t> response_data) {
  switch (return_code) {
    case U2fReturnCode::SUCCESS:
      state_ = State::COMPLETE;
      cb_.Run(return_code, response_data);
      break;
    case U2fReturnCode::CONDITIONS_NOT_SATISFIED:
      // Waiting for user touch, move on and try this device later
      state_ = State::IDLE;
      Transition();
      break;
    default:
      state_ = State::IDLE;
      // An error has occured, quit trying this device
      current_device_ = nullptr;
      Transition();
      break;
  }
}

}  // namespace device
