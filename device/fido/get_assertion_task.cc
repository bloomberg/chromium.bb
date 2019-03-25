// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <utility>

#include "base/bind.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/u2f_sign_operation.h"

namespace device {

namespace {

bool MayFallbackToU2fWithAppIdExtension(
    const FidoDevice& device,
    const CtapGetAssertionRequest& request) {
  bool ctap2_device_supports_u2f =
      device.device_info() &&
      base::ContainsKey(device.device_info()->versions(),
                        ProtocolVersion::kU2f);
  return request.alternative_application_parameter() &&
         ctap2_device_supports_u2f;
}

}  // namespace

GetAssertionTask::GetAssertionTask(FidoDevice* device,
                                   CtapGetAssertionRequest request,
                                   GetAssertionTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      callback_(std::move(callback)),
      weak_factory_(this) {
  // This code assumes that user-presence is requested in order to implement
  // possible U2F-fallback.
  DCHECK(request_.user_presence_required());

  // The UV parameter should have been made binary by this point because CTAP2
  // only takes a binary value.
  DCHECK_NE(request_.user_verification(),
            UserVerificationRequirement::kPreferred);
}

GetAssertionTask::~GetAssertionTask() = default;

void GetAssertionTask::StartTask() {
  if (device()->supported_protocol() == ProtocolVersion::kCtap) {
    GetAssertion();
  } else {
    U2fSign();
  }
}

void GetAssertionTask::GetAssertion() {
  // If appId extension was used in the request and device is a hybrid U2F/CTAP2
  // device, then first issue a silent GetAssertionRequest. If no credentials in
  // allowed credential list are recognized, it's possible that the credential
  // is registered via U2F. Under these circumstances, the request should be
  // issued via the U2F protocol. Otherwise, proceed with a normal GetAssertion
  // request.
  if (MayFallbackToU2fWithAppIdExtension(*device(), request_)) {
    request_.SetUserPresenceRequired(false);
    const auto original_uv_configuration = request_.user_verification();
    request_.SetUserVerification(UserVerificationRequirement::kDiscouraged);

    sign_operation_ = std::make_unique<Ctap2DeviceOperation<
        CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>>(
        device(), request_,
        base::BindOnce(&GetAssertionTask::HandleResponseToSilentRequest,
                       weak_factory_.GetWeakPtr(), original_uv_configuration),
        base::BindOnce(&ReadCTAPGetAssertionResponse));
    sign_operation_->Start();
    return;
  }

  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), request_, std::move(callback_),
          base::BindOnce(&ReadCTAPGetAssertionResponse));
  sign_operation_->Start();
}

void GetAssertionTask::U2fSign() {
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());

  sign_operation_ = std::make_unique<U2fSignOperation>(device(), request_,
                                                       std::move(callback_));
  sign_operation_->Start();
}

void GetAssertionTask::HandleResponseToSilentRequest(
    UserVerificationRequirement original_uv_configuration,
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> response_data) {
  DCHECK(!request_.user_presence_required());
  request_.SetUserPresenceRequired(true);

  if (response_code != CtapDeviceResponseCode::kSuccess) {
    // An error occurred or no credentials in the allowed list were recognized.
    // However, as the relying party has provided appId extension, try again
    // with U2F in case that works.
    DCHECK(MayFallbackToU2fWithAppIdExtension(*device(), request_));
    device()->set_supported_protocol(ProtocolVersion::kU2f);
    U2fSign();
    return;
  }

  // Credential was recognized by the device. As this authentication was a
  // silent authentication (i.e. user touch was not provided), try again with
  // user presence enforced and with the original user verification
  // configuration.
  DCHECK_EQ(UserVerificationRequirement::kDiscouraged,
            request_.user_verification());
  DCHECK_EQ(ProtocolVersion::kCtap, device()->supported_protocol());
  request_.SetUserVerification(original_uv_configuration);

  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), request_, std::move(callback_),
          base::BindOnce(&ReadCTAPGetAssertionResponse));
  sign_operation_->Start();
}

}  // namespace device
