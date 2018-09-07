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

namespace {

// Checks whether the incoming MakeCredential request has ClientPin option that
// is compatible with the Chrome's CTAP2 implementation. According to the CTAP
// spec, CTAP2 authenticators that have client pin set will always error out on
// MakeCredential request when "pinAuth" parameter in the request is not set. As
// ClientPin is not supported on Chrome yet, this check allows Chrome to avoid
// such failures if possible by defaulting to U2F request when user verification
// is not required and the device supports U2F protocol.
// TODO(hongjunchoi): Remove this once ClientPin command is implemented.
// See: https://crbug.com/870892
bool IsClientPinOptionCompatible(const FidoDevice* device,
                                 const CtapMakeCredentialRequest& request) {
  if (request.user_verification_required())
    return true;

  DCHECK(device && device->device_info());
  bool client_pin_set =
      device->device_info()->options().client_pin_availability() ==
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  bool supports_u2f = base::ContainsKey(device->device_info()->versions(),
                                        ProtocolVersion::kU2f);
  return !client_pin_set || !supports_u2f;
}

}  // namespace

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
      device()->supported_protocol() == ProtocolVersion::kCtap &&
      IsClientPinOptionCompatible(device(), request_parameter_)) {
    MakeCredential();
  } else {
    device()->set_supported_protocol(ProtocolVersion::kU2f);
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

  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());
  register_operation_ = std::make_unique<U2fRegisterOperation>(
      device(), request_parameter_, std::move(callback_));
  register_operation_->Start();
}

}  // namespace device
