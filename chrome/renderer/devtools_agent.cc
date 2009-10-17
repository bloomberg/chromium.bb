// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent.h"

#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "webkit/api/public/WebDevToolsAgent.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebString.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebPoint;
using WebKit::WebString;

// static
std::map<int, DevToolsAgent*> DevToolsAgent::agent_for_routing_id_;

DevToolsAgent::DevToolsAgent(int routing_id, RenderView* view)
    : routing_id_(routing_id),
      view_(view) {
  agent_for_routing_id_[routing_id] = this;
}

DevToolsAgent::~DevToolsAgent() {
  agent_for_routing_id_.erase(routing_id_);
}

void DevToolsAgent::OnNavigate() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->didNavigate();
  }
}

// Called on the Renderer thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_RpcMessage, OnRpcMessage)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_SetApuAgentEnabled,
                        OnSetApuAgentEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsAgent::sendMessageToFrontend(const WebString& class_name,
                                          const WebString& method_name,
                                          const WebString& param1,
                                          const WebString& param2,
                                          const WebString& param3) {
  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      routing_id_,
      DevToolsClientMsg_RpcMessage(
          class_name.utf8(),
          method_name.utf8(),
          param1.utf8(),
          param2.utf8(),
          param3.utf8()));
  view_->Send(m);
}

int DevToolsAgent::hostIdentifier() {
  return routing_id_;
}

void DevToolsAgent::forceRepaint() {
  view_->GenerateFullRepaint();
}

// static
DevToolsAgent* DevToolsAgent::FromHostId(int host_id) {
  std::map<int, DevToolsAgent*>::iterator it =
      agent_for_routing_id_.find(host_id);
  if (it != agent_for_routing_id_.end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsAgent::OnAttach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
  }
}

void DevToolsAgent::OnDetach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->detach();
  }
}

void DevToolsAgent::OnRpcMessage(const std::string& class_name,
                                 const std::string& method_name,
                                 const std::string& param1,
                                 const std::string& param2,
                                 const std::string& param3) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->dispatchMessageFromFrontend(
        WebString::fromUTF8(class_name),
        WebString::fromUTF8(method_name),
        WebString::fromUTF8(param1),
        WebString::fromUTF8(param2),
        WebString::fromUTF8(param3));
  }
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    web_agent->inspectElementAt(WebPoint(x, y));
  }
}

void DevToolsAgent::OnSetApuAgentEnabled(bool enabled) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->setApuAgentEnabled(enabled);
  }
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebView* web_view = view_->webview();
  if (!web_view)
    return NULL;
  return web_view->devToolsAgent();
}
