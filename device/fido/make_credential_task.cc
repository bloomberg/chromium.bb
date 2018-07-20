// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_task.h"

#include <utility>

#include "base/bind.h"
#include "device/base/features.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/u2f_register_operation.h"

namespace device {

MakeCredentialTask::MakeCredentialTask(
    FidoDevice* device,
    CtapMakeCredentialRequest request_parameter,
    MakeCredentialTaskCallback callback)
    : FidoTask(device),
      request_parameter_(std::move(request_parameter)),
      callback_(std::move(callback)),
      weak_factory_(this) {}

MakeCredentialTask::~MakeCredentialTask() = default;

void MakeCredentialTask::StartTask() {
  if (base::FeatureList::IsEnabled(kNewCtap2Device) &&
      device()->supported_protocol() == ProtocolVersion::kCtap) {
    MakeCredential();
  } else {
    U2fRegister();
  }
}

void MakeCredentialTask::MakeCredential() {
  register_operation_ = std::make_unique<Ctap2DeviceOperation<
      CtapMakeCredentialRequest, AuthenticatorMakeCredentialResponse>>(
      device(), request_parameter_, std::move(callback_),
      base::BindOnce(&ReadCTAPMakeCredentialResponse));
  register_operation_->Start();
}

void MakeCredentialTask::U2fRegister() {
  if (!IsConvertibleToU2fRegisterCommand(request_parameter_)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  register_operation_ = std::make_unique<U2fRegisterOperation>(
      device(), request_parameter_, std::move(callback_));
  register_operation_->Start();
}

}  // namespace device
