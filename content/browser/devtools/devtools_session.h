// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/forward.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"

namespace content {

class DevToolsAgentHostClient;
class DevToolsAgentHostImpl;
class DevToolsExternalAgentProxyDelegate;

namespace protocol {
class DevToolsDomainHandler;
}

class DevToolsSession : public protocol::FrontendChannel,
                        public blink::mojom::DevToolsSessionHost,
                        public DevToolsExternalAgentProxy {
 public:
  explicit DevToolsSession(DevToolsAgentHostClient* client);
  ~DevToolsSession() override;

  void SetAgentHost(DevToolsAgentHostImpl* agent_host);
  void SetRuntimeResumeCallback(base::OnceClosure runtime_resume);
  void Dispose();

  DevToolsAgentHostClient* client() { return client_; }
  DevToolsSession* GetRootSession();

  // Browser-only sessions do not talk to mojom::DevToolsAgent, but instead
  // handle all protocol messages locally in the browser process.
  void SetBrowserOnly(bool browser_only);
  void AddHandler(std::unique_ptr<protocol::DevToolsDomainHandler> handler);
  void TurnIntoExternalProxy(DevToolsExternalAgentProxyDelegate* delegate);

  void AttachToAgent(blink::mojom::DevToolsAgent* agent);
  bool DispatchProtocolMessage(const std::string& message);
  void SuspendSendingMessagesToAgent();
  void ResumeSendingMessagesToAgent();

  using HandlersMap =
      base::flat_map<std::string,
                     std::unique_ptr<protocol::DevToolsDomainHandler>>;
  HandlersMap& handlers() { return handlers_; }

  static bool IsRuntimeResumeCommand(base::Value* value);
  DevToolsSession* AttachChildSession(const std::string& session_id,
                                      DevToolsAgentHostImpl* agent_host,
                                      DevToolsAgentHostClient* client);
  void DetachChildSession(const std::string& session_id);
  void SendMessageFromChildSession(const std::string& session_id,
                                   const std::string& message);

 private:
  void MojoConnectionDestroyed();
  void DispatchProtocolMessageToAgent(int call_id,
                                      const std::string& method,
                                      const std::string& message);
  void HandleCommand(std::unique_ptr<base::DictionaryValue> parsed_message,
                     const std::string& message);
  bool DispatchProtocolMessageInternal(
      const std::string& message,
      std::unique_ptr<base::DictionaryValue> parsed_message);

  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void flushProtocolNotifications() override;
  void fallThrough(int call_id,
                   const std::string& method,
                   const std::string& message) override;

  // blink::mojom::DevToolsSessionHost implementation.
  void DispatchProtocolResponse(
      const std::string& message,
      int call_id,
      blink::mojom::DevToolsSessionStatePtr updates) override;
  void DispatchProtocolNotification(
      const std::string& message,
      blink::mojom::DevToolsSessionStatePtr updates) override;

  // DevToolsExternalAgentProxy implementation.
  void DispatchOnClientHost(const std::string& message) override;
  void ConnectionClosed() override;

  // Merges the |updates| received from the renderer into session_state_cookie_.
  void ApplySessionStateUpdates(blink::mojom::DevToolsSessionStatePtr updates);

  mojo::AssociatedBinding<blink::mojom::DevToolsSessionHost> binding_;
  blink::mojom::DevToolsSessionAssociatedPtr session_ptr_;
  blink::mojom::DevToolsSessionPtr io_session_ptr_;
  DevToolsAgentHostImpl* agent_host_ = nullptr;
  DevToolsAgentHostClient* client_;
  bool browser_only_ = false;
  HandlersMap handlers_;
  std::unique_ptr<protocol::UberDispatcher> dispatcher_;

  // These messages were queued after suspending, not sent to the agent,
  // and will be sent after resuming.
  struct SuspendedMessage {
    int call_id;
    std::string method;
    std::string message;
  };
  std::vector<SuspendedMessage> suspended_messages_;
  bool suspended_sending_messages_to_agent_ = false;

  // These messages have been sent to agent, but did not get a response yet.
  struct WaitingMessage {
    std::string method;
    std::string message;
  };
  std::map<int, WaitingMessage> waiting_for_response_messages_;

  // |session_state_cookie_| always corresponds to a state before
  // any of the waiting for response messages have been handled.
  // |session_state_cookie_| is nullptr before first attach.
  blink::mojom::DevToolsSessionStatePtr session_state_cookie_;

  DevToolsSession* root_session_ = nullptr;
  base::flat_map<std::string, DevToolsSession*> child_sessions_;
  base::OnceClosure runtime_resume_;
  DevToolsExternalAgentProxyDelegate* proxy_delegate_ = nullptr;

  base::WeakPtrFactory<DevToolsSession> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_SESSION_H_
