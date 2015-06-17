// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_
#define COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_

#include "base/macros.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "third_party/WebKit/public/web/WebDevToolsAgentClient.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace blink {
class WebLocalFrame;
}

namespace mojo {
class Shell;
}

namespace html_viewer {

class DevToolsAgentImpl : public devtools_service::DevToolsAgent,
                          public blink::WebDevToolsAgentClient,
                          public mojo::ErrorHandler {
 public:
  // |frame| must outlive this object.
  DevToolsAgentImpl(blink::WebLocalFrame* frame, mojo::Shell* shell);
  ~DevToolsAgentImpl() override;

  blink::WebLocalFrame* frame() const { return frame_; }

  // Returns whether a "Page.navigate" command is being handled.
  bool handling_page_navigate_request() const {
    return handling_page_navigate_request_;
  }

 private:
  // devtools_service::DevToolsAgent implementation.
  void SetClient(devtools_service::DevToolsAgentClientPtr client,
                 const mojo::String& client_id) override;
  void DispatchProtocolMessage(const mojo::String& message) override;

  // blink::WebDevToolsAgentClient implementation.
  void sendProtocolMessage(int call_id,
                           const blink::WebString& response,
                           const blink::WebString& state);

  // mojo::ErrorHandler implementation.
  void OnConnectionError() override;

  blink::WebLocalFrame* const frame_;
  mojo::Binding<DevToolsAgent> binding_;
  devtools_service::DevToolsAgentClientPtr client_;

  bool handling_page_navigate_request_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAgentImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DEVTOOLS_AGENT_IMPL_H_
