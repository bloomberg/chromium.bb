// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/web_authn.h"

namespace content {
class VirtualFidoDiscoveryFactory;
namespace protocol {

class WebAuthnHandler : public DevToolsDomainHandler, public WebAuthn::Backend {
 public:
  WebAuthnHandler();
  ~WebAuthnHandler() override;

  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // WebAuthn::Backend
  Response Enable() override;
  Response Disable() override;
  Response AddVirtualAuthenticator(
      std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
      String* out_authenticator_id) override;
  Response RemoveVirtualAuthenticator(const String& authenticator_id) override;

 private:
  RenderFrameHostImpl* frame_host_;
  VirtualFidoDiscoveryFactory* virtual_discovery_factory_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(WebAuthnHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_WEBAUTHN_HANDLER_H_
