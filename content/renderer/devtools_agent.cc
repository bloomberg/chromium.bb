// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools_agent.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/string_number_conversions.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/devtools_agent_filter.h"
#include "content/renderer/devtools_client.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebCString;
using WebKit::WebVector;
using WebKit::WebView;

namespace {

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(MessageLoop::current()) { }
  virtual ~WebKitClientMessageLoopImpl() {
    message_loop_ = NULL;
  }
  virtual void run() {
    bool old_state = message_loop_->NestableTasksAllowed();
    message_loop_->SetNestableTasksAllowed(true);
    message_loop_->Run();
    message_loop_->SetNestableTasksAllowed(old_state);
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  MessageLoop* message_loop_;
};

typedef std::map<int, DevToolsAgent*> IdToAgentMap;
base::LazyInstance<IdToAgentMap, base::LeakyLazyInstanceTraits<IdToAgentMap> >
    g_agent_for_routing_id = LAZY_INSTANCE_INITIALIZER;

} //  namespace

DevToolsAgent::DevToolsAgent(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      is_attached_(false) {
  g_agent_for_routing_id.Get()[routing_id()] = this;

  render_view->webview()->setDevToolsAgentClient(this);
  render_view->webview()->devToolsAgent()->setProcessId(
      base::Process::Current().pid());
}

DevToolsAgent::~DevToolsAgent() {
  g_agent_for_routing_id.Get().erase(routing_id());
}

// Called on the Renderer thread.
bool DevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_InspectElement, OnInspectElement)
    IPC_MESSAGE_HANDLER(DevToolsMsg_SetupDevToolsClient, OnSetupDevToolsClient)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == ViewMsg_Navigate::ID)
    OnNavigate();  // Don't want to swallow the message.

  return handled;
}

void DevToolsAgent::sendMessageToInspectorFrontend(
    const WebKit::WebString& message) {
  Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(routing_id(),
                                                         message.utf8()));
}

int DevToolsAgent::hostIdentifier() {
  return routing_id();
}

void DevToolsAgent::saveAgentRuntimeState(
    const WebKit::WebString& state) {
  Send(new DevToolsHostMsg_SaveAgentRuntimeState(routing_id(), state.utf8()));
}

WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
    DevToolsAgent::createClientMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void DevToolsAgent::clearBrowserCache() {
  Send(new DevToolsHostMsg_ClearBrowserCache(routing_id()));
}

void DevToolsAgent::clearBrowserCookies() {
  Send(new DevToolsHostMsg_ClearBrowserCookies(routing_id()));
}

// static
DevToolsAgent* DevToolsAgent::FromHostId(int host_id) {
  IdToAgentMap::iterator it = g_agent_for_routing_id.Get().find(host_id);
  if (it != g_agent_for_routing_id.Get().end()) {
    return it->second;
  }
  return NULL;
}

void DevToolsAgent::OnAttach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    is_attached_ = true;
  }
}

void DevToolsAgent::OnReattach(const std::string& agent_state) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->reattach(WebString::fromUTF8(agent_state));
    is_attached_ = true;
  }
}

void DevToolsAgent::OnDetach() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->detach();
    is_attached_ = false;
  }
}

void DevToolsAgent::OnDispatchOnInspectorBackend(const std::string& message) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->dispatchOnInspectorBackend(WebString::fromUTF8(message));
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach();
    web_agent->inspectElementAt(WebPoint(x, y));
  }
}

void DevToolsAgent::OnNavigate() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->didNavigate();
  }
}

void DevToolsAgent::OnSetupDevToolsClient() {
  new DevToolsClient(static_cast<RenderViewImpl*>(render_view()));
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return NULL;
  return web_view->devToolsAgent();
}

bool DevToolsAgent::IsAttached() {
  return is_attached_;
}
