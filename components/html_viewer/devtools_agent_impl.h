// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_
#define COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"

namespace blink {
class WebLocalFrame;
}

namespace html_viewer {

class DevToolsAgentImpl : public devtools_service::DevToolsAgent,
                          public blink::WebDevToolsAgentClient {
 public:
  // |frame| must outlive this object.
  // This agent should restore its internal state using |state| if it is not
  // null.
  DevToolsAgentImpl(blink::WebLocalFrame* frame,
                    const std::string& id,
                    const std::string* state);
  ~DevToolsAgentImpl() override;

  void BindToRequest(mojo::InterfaceRequest<DevToolsAgent> request);

 private:
  // devtools_service::DevToolsAgent implementation.
  void SetClient(devtools_service::DevToolsAgentClientPtr client) override;
  void DispatchProtocolMessage(const mojo::String& message) override;

  // blink::WebDevToolsAgentClient implementation.
  void sendProtocolMessage(int call_id,
                           const blink::WebString& response,
                           const blink::WebString& state);

  void OnConnectionError();

  blink::WebLocalFrame* const frame_;
  const std::string id_;

  mojo::Binding<DevToolsAgent> binding_;
  devtools_service::DevToolsAgentClientPtr client_;

  // If we restore the agent's internal state using serialized state data from a
  // previous agent, the agent may generate messages before |client_| is set.
  // In that case, we need to cache messages for the client.
  bool cache_until_client_ready_;

  struct CachedClientMessage {
    int call_id;
    mojo::String response;
    mojo::String state;
  };
  std::vector<CachedClientMessage> cached_client_messages_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_
