// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_task.h"

namespace device {

// Represents one register operation on one single CTAP 1.0/2.0 authenticator.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatormakecredential
class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialTask : public FidoTask {
 public:
  using MakeCredentialTaskCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorMakeCredentialResponse>)>;
  using RegisterOperation =
      DeviceOperation<CtapMakeCredentialRequest,
                      AuthenticatorMakeCredentialResponse>;

  MakeCredentialTask(FidoDevice* device,
                     CtapMakeCredentialRequest request_parameter,
                     MakeCredentialTaskCallback callback);
  ~MakeCredentialTask() override;

 private:
  // FidoTask:
  void StartTask() final;

  void MakeCredential();
  void U2fRegister();
  void OnCtapMakeCredentialResponseReceived(
      CtapDeviceResponseCode return_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response_data);

  CtapMakeCredentialRequest request_parameter_;
  std::unique_ptr<RegisterOperation> register_operation_;
  MakeCredentialTaskCallback callback_;
  base::WeakPtrFactory<MakeCredentialTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialTask);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_TASK_H_
