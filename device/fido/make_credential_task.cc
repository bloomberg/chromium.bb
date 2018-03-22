// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_task.h"

#include <utility>

#include "base/bind.h"
#include "device/fido/ctap_empty_authenticator_request.h"
#include "device/fido/device_response_converter.h"

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
  GetAuthenticatorInfo(base::BindOnce(&MakeCredentialTask::MakeCredential,
                                      weak_factory_.GetWeakPtr()),
                       base::BindOnce(&MakeCredentialTask::U2fRegister,
                                      weak_factory_.GetWeakPtr()));
}

void MakeCredentialTask::MakeCredential() {
  device()->DeviceTransact(
      request_parameter_.EncodeAsCBOR(),
      base::BindOnce(&MakeCredentialTask::OnCtapMakeCredentialResponseReceived,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialTask::U2fRegister() {
  // TODO(hongjunchoi): Implement U2F register request logic to support
  // interoperability with U2F protocol. Currently all U2F devices are not
  // supported and request to U2F devices will be silently dropped.
  // See: https://crbug.com/798573
  std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                           base::nullopt);
}

void MakeCredentialTask::OnCtapMakeCredentialResponseReceived(
    base::Optional<std::vector<uint8_t>> device_response) {
  if (!device_response) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  std::move(callback_).Run(GetResponseCode(*device_response),
                           ReadCTAPMakeCredentialResponse(*device_response));
}

}  // namespace device
