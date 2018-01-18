// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_

#include <memory>

#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/protocol.h"

namespace content {
class DevToolsAgentHost;
class DevToolsAgentHostClient;
}

class BrowserHandler;
class PageHandler;
class WindowManagerHandler;

class ChromeDevToolsSession : public protocol::FrontendChannel {
 public:
  ChromeDevToolsSession(content::DevToolsAgentHost* agent_host,
                        content::DevToolsAgentHostClient* client);
  ~ChromeDevToolsSession() override;

  protocol::UberDispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  // protocol::FrontendChannel:
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;

  content::DevToolsAgentHost* const agent_host_;
  content::DevToolsAgentHostClient* const client_;

  std::unique_ptr<protocol::UberDispatcher> dispatcher_;
  std::unique_ptr<BrowserHandler> browser_handler_;
  std::unique_ptr<PageHandler> page_handler_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<WindowManagerHandler> window_manager_protocl_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsSession);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_H_
