// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/devtools_agent_host.h"

namespace net {
class ServerSocket;
}

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace content {
namespace devtools {
namespace browser {

class BrowserHandler : public DevToolsAgentHostClient {
 public:
  using Response = DevToolsProtocolClient::Response;

  BrowserHandler();
  ~BrowserHandler() override;

  void SetClient(std::unique_ptr<Client> client);
  void Detached();

  Response CreateBrowserContext(std::string* out_context_id);
  Response DisposeBrowserContext(const std::string& context_id,
                                 bool* out_success);
  Response CreateTarget(const std::string& url,
                        const int* width,
                        const int* height,
                        const std::string* context_id,
                        std::string* out_target_id);
  Response CloseTarget(const std::string& target_id, bool* out_success);
  Response GetTargets(DevToolsCommandId command_id);
  Response Attach(DevToolsCommandId command_id,
                  const std::string& target_id);
  Response Detach(const std::string& target_id, bool* out_success);
  Response SendMessage(const std::string& target_id,
                       const std::string& message);
  Response SetRemoteLocations(
      const std::vector<std::unique_ptr<base::DictionaryValue>>&);

 private:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override;

  void AgentHostClosed(DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  void RespondToGetTargets(DevToolsCommandId command_id,
                           DevToolsAgentHost::List list);
  void RespondToAttach(DevToolsCommandId command_id,
                       const std::string& target_id,
                       DevToolsAgentHost::List agents);

  std::unique_ptr<Client> client_;
  DevToolsAgentHost::List attached_hosts_;
  base::WeakPtrFactory<BrowserHandler> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};

}  // namespace browser
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_BROWSER_HANDLER_H_
