// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/protocol.h"

namespace content {

class DevToolsAgentHostClient;
class RenderFrameHostImpl;

class DevToolsSession : public protocol::FrontendChannel {
 public:
  DevToolsSession(DevToolsAgentHostImpl* agent_host,
                  DevToolsAgentHostClient* client,
                  int session_id);
  ~DevToolsSession() override;

  int session_id() const { return session_id_; }
  void AddHandler(std::unique_ptr<protocol::DevToolsDomainHandler> handler);
  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetFallThroughForNotFound(bool value);

  protocol::Response::Status Dispatch(
      const std::string& message,
      int* call_id,
      std::string* method);

  // Only used by DevToolsAgentHostImpl.
  DevToolsAgentHostClient* client() const { return client_; }

  template <typename Handler>
  static std::vector<Handler*> HandlersForAgentHost(
      DevToolsAgentHostImpl* agent_host,
      const std::string& name) {
    std::vector<Handler*> result;
    if (agent_host->sessions_.empty())
      return result;
    for (const auto& session : agent_host->sessions_) {
      auto it = session->handlers_.find(name);
      if (it != session->handlers_.end())
        result.push_back(static_cast<Handler*>(it->second.get()));
    }
    return result;
  }

 private:
  void sendResponse(std::unique_ptr<base::DictionaryValue> response);
  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;

  DevToolsAgentHostImpl* agent_host_;
  DevToolsAgentHostClient* client_;
  int session_id_;
  base::flat_map<std::string, std::unique_ptr<protocol::DevToolsDomainHandler>>
      handlers_;
  RenderFrameHostImpl* host_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_;

  base::WeakPtrFactory<DevToolsSession> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
