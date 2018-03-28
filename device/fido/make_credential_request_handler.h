// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class FidoDevice;
class AuthenticatorMakeCredentialResponse;

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandler {
 public:
  using RegisterResponseCallback = base::OnceCallback<void(
      FidoReturnCode status_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response_data)>;

  MakeCredentialRequestHandler(
      service_manager::Connector* connector,
      const base::flat_set<U2fTransportProtocol>& protocols,
      CtapMakeCredentialRequest request_parameter,
      AuthenticatorSelectionCriteria authenticator_criteria,
      RegisterResponseCallback completion_callback);
  ~MakeCredentialRequestHandler() override;

 private:
  // FidoRequestHandler:
  std::unique_ptr<FidoTask> CreateTaskForNewDevice(FidoDevice* device) final;

  // Converts device response code received from CTAP1/CTAP2 device into
  // FidoReturnCode and passes AuthenticatorMakeCredentialResponse data to
  // webauth::mojom::Authenticator.
  void DispatchResponse(
      FidoDevice* device,
      CtapDeviceResponseCode return_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response_data);

  CtapMakeCredentialRequest request_parameter_;
  RegisterResponseCallback completion_callback_;
  AuthenticatorSelectionCriteria authenticator_selection_criteria_;
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
