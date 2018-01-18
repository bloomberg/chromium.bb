// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/WebKit/public/web/devtools_agent.mojom.h"

namespace content {

class DevToolsAgentHostClient;
class RenderFrameHostImpl;

class DevToolsSession : public protocol::FrontendChannel,
                        public blink::mojom::DevToolsSessionHost {
 public:
  DevToolsSession(DevToolsAgentHostImpl* agent_host,
                  DevToolsAgentHostClient* client);
  ~DevToolsSession() override;

  DevToolsAgentHostClient* client() const { return client_; }
  void AddHandler(std::unique_ptr<protocol::DevToolsDomainHandler> handler);
  void SetRenderer(RenderProcessHost* process_host,
                   RenderFrameHostImpl* frame_host);
  void SetFallThroughForNotFound(bool value);
  void AttachToAgent(const blink::mojom::DevToolsAgentAssociatedPtr& agent);
  void ReattachToAgent(const blink::mojom::DevToolsAgentAssociatedPtr& agent);

  struct Message {
    std::string method;
    std::string message;
  };
  using MessageByCallId = std::map<int, Message>;
  MessageByCallId& waiting_messages() { return waiting_for_response_messages_; }
  const std::string& state_cookie() { return state_cookie_; }

  protocol::Response::Status Dispatch(
      const std::string& message,
      int* call_id,
      std::string* method);
  void DispatchProtocolMessageToAgent(int call_id,
                                      const std::string& method,
                                      const std::string& message);
  void InspectElement(const gfx::Point& point);

  template <typename Handler>
  static std::vector<Handler*> HandlersForAgentHost(
      DevToolsAgentHostImpl* agent_host,
      const std::string& name) {
    std::vector<Handler*> result;
    if (agent_host->sessions().empty())
      return result;
    for (DevToolsSession* session : agent_host->sessions()) {
      auto it = session->handlers_.find(name);
      if (it != session->handlers_.end())
        result.push_back(static_cast<Handler*>(it->second.get()));
    }
    return result;
  }

 private:
  void SendResponse(std::unique_ptr<base::DictionaryValue> response);
  void MojoConnectionDestroyed();
  void ReceivedBadMessage();

  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;

  // blink::mojom::DevToolsSessionHost implementation.
  void DispatchProtocolMessage(
      blink::mojom::DevToolsMessageChunkPtr chunk) override;

  mojo::AssociatedBinding<blink::mojom::DevToolsSessionHost> binding_;
  blink::mojom::DevToolsSessionAssociatedPtr session_ptr_;
  blink::mojom::DevToolsSessionPtr io_session_ptr_;
  DevToolsAgentHostImpl* agent_host_;
  DevToolsAgentHostClient* client_;
  base::flat_map<std::string, std::unique_ptr<protocol::DevToolsDomainHandler>>
      handlers_;
  RenderProcessHost* process_;
  RenderFrameHostImpl* host_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_;
  MessageByCallId waiting_for_response_messages_;

  // |state_cookie_| always corresponds to a state before
  // any of the waiting for response messages have been handled.
  std::string state_cookie_;
  std::string response_message_buffer_;

  base::WeakPtrFactory<DevToolsSession> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
