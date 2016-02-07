// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_H_
#define COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "components/web_view/frame.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class DictionaryValue;
}

namespace mojo {
class Shell;
}

namespace web_view {

class FrameDevToolsAgentDelegate;

// FrameDevToolsAgent relays messages between the DevTools service and the
// DevTools agent of the frame that it attaches to.
// It persists state across frame navigations and also intercepts
// "Page.navigate" requests.
class FrameDevToolsAgent : public devtools_service::DevToolsAgent {
 public:
  FrameDevToolsAgent(mojo::Shell* shell, FrameDevToolsAgentDelegate* delegate);
  ~FrameDevToolsAgent() override;

  // Attaches this agent to a new frame. |forward_agent| is the DevToolsAgent
  // interface provided by the frame. DevTools-related properties are added to
  // |devtools_properties|, which should be passed to the frame to initialize
  // its DevTools agent.
  void AttachFrame(devtools_service::DevToolsAgentPtr forward_agent,
                   Frame::ClientPropertyMap* devtools_properties);

 private:
  class FrameDevToolsAgentClient;

  void RegisterAgentIfNecessary();

  void HandlePageNavigateRequest(const base::DictionaryValue* request);

  // devtools_service::DevToolsAgent implementation.
  void SetClient(devtools_service::DevToolsAgentClientPtr client) override;
  void DispatchProtocolMessage(const mojo::String& message) override;

  // The following methods are called by |client_impl_|.
  void OnReceivedClientMessage(int32_t call_id,
                               const mojo::String& message,
                               const mojo::String& state);
  void OnForwardClientClosed();

  mojo::Shell* const shell_;
  FrameDevToolsAgentDelegate* const delegate_;

  const std::string id_;

  scoped_ptr<FrameDevToolsAgentClient> client_impl_;

  mojo::Binding<DevToolsAgent> binding_;
  // The DevToolsAgent interface provided by the frame. This class will forward
  // DevToolsAgent method calls it receives to |forward_agent_|.
  devtools_service::DevToolsAgentPtr forward_agent_;

  std::string state_;
  std::map<int, std::string> pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(FrameDevToolsAgent);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_DEVTOOLS_AGENT_H_
