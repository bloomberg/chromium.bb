// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2f_device.h"

#include "base/bind.h"
#include "u2f_apdu_command.h"
#include "u2f_apdu_response.h"

namespace device {

U2fDevice::U2fDevice() : channel_id_(kBroadcastChannel), capabilities_(0) {}

U2fDevice::~U2fDevice() {}

void U2fDevice::Register(const std::vector<uint8_t>& app_param,
                         const std::vector<uint8_t>& challenge_param,
                         const MessageCallback& callback) {
  std::unique_ptr<U2fApduCommand> register_cmd =
      U2fApduCommand::CreateRegister(app_param, challenge_param);
  if (!register_cmd) {
    callback.Run(U2fReturnCode::INVALID_PARAMS, std::vector<uint8_t>());
    return;
  }
  DeviceTransact(
      std::move(register_cmd),
      base::Bind(&U2fDevice::OnRegisterComplete, GetWeakPtr(), callback));
}

void U2fDevice::Sign(const std::vector<uint8_t>& app_param,
                     const std::vector<uint8_t>& challenge_param,
                     const std::vector<uint8_t>& key_handle,
                     const MessageCallback& callback,
                     bool check_only) {
  std::unique_ptr<U2fApduCommand> sign_cmd = U2fApduCommand::CreateSign(
      app_param, challenge_param, key_handle, check_only);
  if (!sign_cmd) {
    callback.Run(U2fReturnCode::INVALID_PARAMS, std::vector<uint8_t>());
    return;
  }
  DeviceTransact(std::move(sign_cmd), base::Bind(&U2fDevice::OnSignComplete,
                                                 GetWeakPtr(), callback));
}

void U2fDevice::Version(const VersionCallback& callback) {
  std::unique_ptr<U2fApduCommand> version_cmd = U2fApduCommand::CreateVersion();
  if (!version_cmd) {
    callback.Run(false, ProtocolVersion::UNKNOWN);
    return;
  }
  DeviceTransact(
      std::move(version_cmd),
      base::Bind(&U2fDevice::OnVersionComplete, GetWeakPtr(), callback));
}

void U2fDevice::OnRegisterComplete(
    const MessageCallback& callback,
    bool success,
    std::unique_ptr<U2fApduResponse> register_response) {
  if (!success || !register_response) {
    callback.Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
    return;
  }
  switch (register_response->status()) {
    case U2fApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      callback.Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED,
                   std::vector<uint8_t>());
      break;
    case U2fApduResponse::Status::SW_NO_ERROR:
      callback.Run(U2fReturnCode::SUCCESS, register_response->data());
      break;
    case U2fApduResponse::Status::SW_WRONG_DATA:
      callback.Run(U2fReturnCode::INVALID_PARAMS, std::vector<uint8_t>());
      break;
    default:
      callback.Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
      break;
  }
}

void U2fDevice::OnSignComplete(const MessageCallback& callback,
                               bool success,
                               std::unique_ptr<U2fApduResponse> sign_response) {
  if (!success || !sign_response) {
    callback.Run(U2fReturnCode::FAILURE, std::vector<uint8_t>());
    return;
  }
  switch (sign_response->status()) {
    case U2fApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      callback.Run(U2fReturnCode::CONDITIONS_NOT_SATISFIED,
                   std::vector<uint8_t>());
      break;
    case U2fApduResponse::Status::SW_NO_ERROR:
      callback.Run(U2fReturnCode::SUCCESS, sign_response->data());
      break;
    case U2fApduResponse::Status::SW_WRONG_DATA:
    case U2fApduResponse::Status::SW_WRONG_LENGTH:
    default:
      callback.Run(U2fReturnCode::INVALID_PARAMS, std::vector<uint8_t>());
      break;
  }
}

void U2fDevice::OnVersionComplete(
    const VersionCallback& callback,
    bool success,
    std::unique_ptr<U2fApduResponse> version_response) {
  if (success && version_response &&
      version_response->status() == U2fApduResponse::Status::SW_NO_ERROR &&
      version_response->data() ==
          std::vector<uint8_t>({'U', '2', 'F', '_', 'V', '2'})) {
    callback.Run(success, ProtocolVersion::U2F_V2);
    return;
  }
  callback.Run(success, ProtocolVersion::UNKNOWN);
}

void U2fDevice::OnLegacyVersionComplete(
    const VersionCallback& callback,
    bool success,
    std::unique_ptr<U2fApduResponse> legacy_version_response) {
  if (success && legacy_version_response &&
      legacy_version_response->status() ==
          U2fApduResponse::Status::SW_NO_ERROR &&
      legacy_version_response->data() ==
          std::vector<uint8_t>({'U', '2', 'F', '_', 'V', '2'})) {
    callback.Run(success, ProtocolVersion::U2F_V2);
    return;
  }
  callback.Run(success, ProtocolVersion::UNKNOWN);
}

}  // namespace device
