// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "content/browser/webauth/register_response_data.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/modules/webauth/authenticator.mojom.h"
#include "url/origin.h"

namespace device {
class U2fRequest;
enum class U2fReturnCode : uint8_t;
}  // namespace device

namespace content {

class RenderFrameHost;

// Implementation of the public Authenticator interface.
class CONTENT_EXPORT AuthenticatorImpl : public webauth::mojom::Authenticator {
 public:
  explicit AuthenticatorImpl(RenderFrameHost* render_frame_host);
  ~AuthenticatorImpl() override;

  // Creates a binding between this object and |request|. Note that a
  // AuthenticatorImpl instance can be bound to multiple requests (as happens in
  // the case of simultaneous starting and finishing operations).
  void Bind(webauth::mojom::AuthenticatorRequest request);

 private:
  // mojom:Authenticator
  void MakeCredential(webauth::mojom::MakeCredentialOptionsPtr options,
                      MakeCredentialCallback callback) override;

  // Callback to handle the async response from a U2fDevice.
  void OnDeviceResponse(MakeCredentialCallback callback,
                        std::unique_ptr<CollectedClientData> client_data,
                        device::U2fReturnCode status_code,
                        const std::vector<uint8_t>& data);

  // Runs when timer expires and cancels all issued requests to a U2fDevice.
  void OnTimeout(
      base::OnceCallback<void(webauth::mojom::AuthenticatorStatus,
                              webauth::mojom::PublicKeyCredentialInfoPtr)>
          callback);

  // Owns pipes to this Authenticator from |render_frame_host_|.
  mojo::BindingSet<webauth::mojom::Authenticator> bindings_;
  std::unique_ptr<device::U2fRequest> u2f_request_;
  base::OneShotTimer timer_;
  RenderFrameHost* render_frame_host_;
  base::WeakPtrFactory<AuthenticatorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_IMPL_H_
