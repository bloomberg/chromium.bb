// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/webauth/collected_client_data.h"
#include "content/common/content_export.h"
#include "device/fido/register_response_data.h"
#include "device/fido/sign_response_data.h"
#include "device/fido/u2f_transport_protocol.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/webauth/authenticator.mojom.h"
#include "url/origin.h"

namespace base {
class OneShotTimer;
}

namespace device {
class U2fRequest;
enum class U2fReturnCode : uint8_t;
}  // namespace device

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

class RenderFrameHost;

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl : public webauth::mojom::Authenticator {
 public:
  explicit AuthenticatorImpl(RenderFrameHost* render_frame_host);

  // Permits setting connector and timer for testing. Using this constructor
  // will also empty out the protocol set, since no device discovery will take
  // place during tests.
  AuthenticatorImpl(RenderFrameHost* render_frame_host,
                    service_manager::Connector*,
                    std::unique_ptr<base::OneShotTimer>);
  ~AuthenticatorImpl() override;

  // Creates a binding between this object and |request|. Note that a
  // AuthenticatorImpl instance can be bound to multiple requests (as happens in
  // the case of simultaneous starting and finishing operations).
  void Bind(webauth::mojom::AuthenticatorRequest request);

 private:
  webauth::mojom::AuthenticatorStatus InitializeAndValidateRequest(
      const std::string& relying_party_id);

  // mojom:Authenticator
  void MakeCredential(webauth::mojom::MakePublicKeyCredentialOptionsPtr options,
                      MakeCredentialCallback callback) override;

  void GetAssertion(
      webauth::mojom::PublicKeyCredentialRequestOptionsPtr options,
      GetAssertionCallback callback) override;

  // Callback to handle the async response from a U2fDevice.
  void OnRegisterResponse(
      device::U2fReturnCode status_code,
      base::Optional<device::RegisterResponseData> response_data);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(device::U2fReturnCode status_code,
                      base::Optional<device::SignResponseData> response_data);

  // Runs when timer expires and cancels all issued requests to a U2fDevice.
  void OnTimeout();
  void Cleanup();

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::BindingSet<webauth::mojom::Authenticator> bindings_;
  std::unique_ptr<device::U2fRequest> u2f_request_;

  // Support both HID and BLE.
  base::flat_set<device::U2fTransportProtocol> protocols_ = {
      device::U2fTransportProtocol::kUsbHumanInterfaceDevice,
      device::U2fTransportProtocol::kBluetoothLowEnergy};

  MakeCredentialCallback make_credential_response_callback_;
  GetAssertionCallback get_assertion_response_callback_;

  // Holds the client data to be returned to the caller.
  CollectedClientData client_data_;
  webauth::mojom::AttestationConveyancePreference attestation_preference_;
  std::unique_ptr<base::OneShotTimer> timer_;
  RenderFrameHost* render_frame_host_;
  service_manager::Connector* connector_ = nullptr;
  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
