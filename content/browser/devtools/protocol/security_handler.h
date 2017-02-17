// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/security.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHostImpl;

namespace protocol {

class SecurityHandler : public DevToolsDomainHandler,
                        public Security::Backend,
                        public WebContentsObserver {
 public:
  SecurityHandler();
  ~SecurityHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;

  static SecurityHandler* FromAgentHost(DevToolsAgentHostImpl* host);

  Response Enable() override;
  Response Disable() override;
  Response ShowCertificateViewer() override;

 private:
  void AttachToRenderFrameHost();

  // WebContentsObserver overrides
  void DidChangeVisibleSecurityState() override;

  std::unique_ptr<Security::Frontend> frontend_;
  bool enabled_;
  RenderFrameHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(SecurityHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
