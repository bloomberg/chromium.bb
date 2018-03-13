// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_device.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/u2f_apdu_command.h"
#include "device/fido/u2f_apdu_response.h"
#include "device/fido/u2f_request.h"

namespace device {

constexpr base::TimeDelta U2fDevice::kDeviceTimeout;

U2fDevice::U2fDevice() = default;

U2fDevice::~U2fDevice() = default;

void U2fDevice::Register(base::Optional<std::vector<uint8_t>> register_cmd,
                         MessageCallback callback) {
  if (!register_cmd) {
    std::move(callback).Run(U2fReturnCode::INVALID_PARAMS,
                            std::vector<uint8_t>());
    return;
  }
  DeviceTransact(std::move(*register_cmd),
                 base::BindOnce(&U2fDevice::OnRegisterComplete, GetWeakPtr(),
                                std::move(callback)));
}

void U2fDevice::Sign(base::Optional<std::vector<uint8_t>> sign_cmd,
                     MessageCallback callback) {
  if (!sign_cmd) {
    std::move(callback).Run(U2fReturnCode::INVALID_PARAMS,
                            std::vector<uint8_t>());
    return;
  }
  DeviceTransact(std::move(*sign_cmd),
                 base::BindOnce(&U2fDevice::OnSignComplete, GetWeakPtr(),
                                std::move(callback)));
}

void U2fDevice::Version(VersionCallback callback) {
  DeviceTransact(U2fRequest::GetU2fVersionApduCommand(),
                 base::BindOnce(&U2fDevice::OnVersionComplete, GetWeakPtr(),
                                std::move(callback), false /* legacy */));
}

void U2fDevice::OnRegisterComplete(
    MessageCallback callback,
    bool success,
    std::unique_ptr<U2fApduResponse> register_response) {
  if (!success || !register_response) {
    std::move(callback).Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
    return;
  }
  switch (register_response->status()) {
    case U2fApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      std::move(callback).Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED,
                              std::vector<uint8_t>());
      break;
    case U2fApduResponse::Status::SW_NO_ERROR:
      std::move(callback).Run(U2fReturnCode::SUCCESS,
                              register_response->data());
      break;
    case U2fApduResponse::Status::SW_WRONG_DATA:
      std::move(callback).Run(U2fReturnCode::INVALID_PARAMS,
                              std::vector<uint8_t>());
      break;
    default:
      std::move(callback).Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
      break;
  }
}

void U2fDevice::OnSignComplete(MessageCallback callback,
                               bool success,
                               std::unique_ptr<U2fApduResponse> sign_response) {
  if (!success || !sign_response) {
    std::move(callback).Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
    return;
  }
  switch (sign_response->status()) {
    case U2fApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      std::move(callback).Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED,
                              std::vector<uint8_t>());
      break;
    case U2fApduResponse::Status::SW_NO_ERROR:
      std::move(callback).Run(U2fReturnCode::SUCCESS, sign_response->data());
      break;
    case U2fApduResponse::Status::SW_WRONG_DATA:
    case U2fApduResponse::Status::SW_WRONG_LENGTH:
    default:
      std::move(callback).Run(U2fReturnCode::INVALID_PARAMS,
                              std::vector<uint8_t>());
      break;
  }
}

void U2fDevice::OnVersionComplete(
    VersionCallback callback,
    bool legacy,
    bool success,
    std::unique_ptr<U2fApduResponse> version_response) {
  if (success && version_response &&
      version_response->status() == U2fApduResponse::Status::SW_NO_ERROR &&
      version_response->data() ==
          std::vector<uint8_t>({'U', '2', 'F', '_', 'V', '2'})) {
    std::move(callback).Run(success, ProtocolVersion::U2F_V2);
  } else if (!legacy) {
    // Standard GetVersion failed, attempt legacy GetVersion command.
    DeviceTransact(U2fRequest::GetU2fVersionApduCommand(true /* legacy */),
                   base::BindOnce(&U2fDevice::OnVersionComplete, GetWeakPtr(),
                                  std::move(callback), true /* legacy */));
  } else {
    std::move(callback).Run(success, ProtocolVersion::UNKNOWN);
  }
}

}  // namespace device
