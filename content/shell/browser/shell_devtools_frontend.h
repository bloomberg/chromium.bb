// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
#define CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace content {

class RenderViewHost;
class Shell;
class WebContents;

class ShellDevToolsFrontend : public WebContentsObserver,
                              public DevToolsFrontendHost::Delegate,
                              public DevToolsAgentHostClient,
                              public net::URLFetcherDelegate {
 public:
  static ShellDevToolsFrontend* Show(WebContents* inspected_contents);

  void Activate();
  void Focus();
  void InspectElementAt(int x, int y);
  void Close();

  void DisconnectFromTarget();

  Shell* frontend_shell() const { return frontend_shell_; }

 protected:
  ShellDevToolsFrontend(Shell* frontend_shell, DevToolsAgentHost* agent_host);
  ~ShellDevToolsFrontend() override;

  // content::DevToolsAgentHostClient implementation.
  void AgentHostClosed(DevToolsAgentHost* agent_host, bool replaced) override;
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AttachTo(WebContents* inspected_contents);

 private:
  // WebContentsObserver overrides
  void RenderViewCreated(RenderViewHost* render_view_host) override;
  void DidNavigateMainFrame(
      const LoadCommittedDetails& details,
      const FrameNavigateParams& params) override;
  void WebContentsDestroyed() override;

  // content::DevToolsFrontendHost::Delegate implementation.
  void HandleMessageFromDevToolsFrontend(const std::string& message) override;
  void HandleMessageFromDevToolsFrontendToBackend(
      const std::string& message) override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  Shell* frontend_shell_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  scoped_ptr<DevToolsFrontendHost> frontend_host_;
  using PendingRequestsMap = std::map<const net::URLFetcher*, int>;
  PendingRequestsMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(ShellDevToolsFrontend);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_FRONTEND_H_
