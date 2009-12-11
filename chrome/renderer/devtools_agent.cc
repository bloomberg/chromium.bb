// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_agent.h"

#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/devtools_agent_filter.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/devtools/devtools_message_data.h"
#include "webkit/glue/glue_util.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

// static
std::map<int, DevToolsAgent*> DevToolsAgent::agent_for_routing_id_;

DevToolsAgent::DevToolsAgent(int routing_id, RenderView* render_view)
    : routing_id_(routing_id),
      render_view_(render_view) {
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

void DevToolsAgent::sendMessageToFrontend(
    const WebKit::WebDevToolsMessageData& data) {
  IPC::Message* m = new ViewHostMsg_ForwardToDevToolsClient(
      routing_id_,
      DevToolsClientMsg_RpcMessage(DevToolsMessageData(data)));
  render_view_->Send(m);
}

int DevToolsAgent::hostIdentifier() {
  return routing_id_;
}

void DevToolsAgent::forceRepaint() {
  render_view_->GenerateFullRepaint();
}

void DevToolsAgent::runtimeFeatureStateChanged(const WebKit::WebString& feature,
                                               bool enabled) {
  render_view_->Send(new ViewHostMsg_DevToolsRuntimeFeatureStateChanged(
      routing_id_,
      feature.utf8(),
      enabled));
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

void DevToolsAgent::OnAttach(const std::vector<std::string>& runtime_features) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    for (std::vector<std::string>::const_iterator it = runtime_features.begin();
         it != runtime_features.end(); ++it) {
      web_agent->setRuntimeFeatureEnabled(WebString::fromUTF8(*it), true);
    }
  }
}

void DevToolsAgent::OnDetach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->detach();
  }
}

void DevToolsAgent::OnRpcMessage(const DevToolsMessageData& data) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->dispatchMessageFromFrontend(data.ToWebDevToolsMessageData());
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
    web_agent->setRuntimeFeatureEnabled(
        webkit_glue::StdStringToWebString("apu-agent"),
        enabled);
  }
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebView* web_view = render_view_->webview();
  if (!web_view)
    return NULL;
  return web_view->devToolsAgent();
}

// static
void WebKit::WebDevToolsAgentClient::sendMessageToFrontendOnIOThread(
    const WebDevToolsMessageData& data) {
    DevToolsAgentFilter::SendRpcMessage(DevToolsMessageData(data));
}
