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

GetAssertionTask::GetAssertionTask(FidoDevice* device,
                                   CtapGetAssertionRequest request,
                                   GetAssertionTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetAssertionTask::~GetAssertionTask() = default;

void GetAssertionTask::StartTask() {
  if (base::FeatureList::IsEnabled(kNewCtap2Device) &&
      device()->supported_protocol() == ProtocolVersion::kCtap) {
    GetAssertion();
  } else {
    U2fSign();
  }
}

void GetAssertionTask::GetAssertion() {
  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), request_, std::move(callback_),
          base::BindOnce(&ReadCTAPGetAssertionResponse));
  sign_operation_->Start();
}

void GetAssertionTask::U2fSign() {
  DCHECK(!device()->device_info());
  DCHECK_EQ(ProtocolVersion::kU2f, device()->supported_protocol());

  sign_operation_ = std::make_unique<U2fSignOperation>(device(), request_,
                                                       std::move(callback_));
  sign_operation_->Start();
}

}  // namespace device
