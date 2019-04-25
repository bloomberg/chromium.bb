// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/pin.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class FidoAuthenticator;

// CredentialManagementHandler implements the authenticatorCredentialManagement
// protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) CredentialManagementHandler
    : public FidoRequestHandlerBase {
 public:
  using GetPINCallback = base::OnceCallback<void(base::Optional<int64_t>)>;

  class Delegate {
   public:
    // TODO(martinkr): These should probably be combined, but separating them is
    // more convenient for development right now.
    virtual void OnCredentialMetadata(size_t num_existing,
                                      size_t num_remaining) = 0;
    virtual void OnCredentialsEnumerated(
        std::vector<AggregatedEnumerateCredentialsResponse> credentials) = 0;
    virtual void OnError(FidoReturnCode) = 0;
  };

  CredentialManagementHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      Delegate* delegate);
  ~CredentialManagementHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kGettingRetries,
    kWaitingForPIN,
    kGettingEphemeralKey,
    kGettingPINToken,
    kGettingMetadata,
    kGettingRP,
    kGettingCredentials,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHavePIN(std::string pin);
  void OnHaveEphemeralKey(std::string pin,
                          CtapDeviceResponseCode status,
                          base::Optional<pin::KeyAgreementResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);
  void OnCredentialsMetadata(
      CtapDeviceResponseCode status,
      base::Optional<CredentialsMetadataResponse> response);

  SEQUENCE_CHECKER(sequence_checker_);

  Delegate* const delegate_;
  State state_ = State::kWaitingForTouch;
  FidoAuthenticator* authenticator_ = nullptr;

  base::WeakPtrFactory<CredentialManagementHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagementHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
