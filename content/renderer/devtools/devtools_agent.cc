// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent.h"

#include <map>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/string_number_conversions.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/devtools/devtools_client.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

using WebKit::WebConsoleMessage;
using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsAgentClient;
using WebKit::WebFrame;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebCString;
using WebKit::WebVector;
using WebKit::WebView;

using base::debug::TraceLog;

namespace content {

namespace {

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(MessageLoop::current()) { }
  virtual ~WebKitClientMessageLoopImpl() {
    message_loop_ = NULL;
  }
  virtual void run() {
    MessageLoop::ScopedNestableTaskAllower allow(message_loop_);
    message_loop_->Run();
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  MessageLoop* message_loop_;
};

typedef std::map<int, DevToolsAgent*> IdToAgentMap;
base::LazyInstance<IdToAgentMap>::Leaky
    g_agent_for_routing_id = LAZY_INSTANCE_INITIALIZER;

} //  namespace

DevToolsAgent::DevToolsAgent(RenderViewImpl* render_view)
    : RenderViewObserver(render_view), is_attached_(false) {
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
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_AddMessageToConsole,
                        OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(DevToolsMsg_SetupDevToolsClient, OnSetupDevToolsClient)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (message.type() == ViewMsg_Navigate::ID ||
      message.type() == ViewMsg_Close::ID)
    ContinueProgram();  // Don't want to swallow the message.

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

void DevToolsAgent::setTraceEventCallback(TraceEventCallback cb) {
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEventCallback(cb);
  trace_log->SetEnabled(!!cb, TraceLog::RECORD_UNTIL_FULL);
}

#if defined(USE_TCMALLOC) && !defined(OS_WIN)
static void AllocationVisitor(void* data, const void* ptr) {
    typedef WebKit::WebDevToolsAgentClient::AllocatedObjectVisitor Visitor;
    Visitor* visitor = reinterpret_cast<Visitor*>(data);
    visitor->visitObject(ptr);
}
#endif

void DevToolsAgent::visitAllocatedObjects(AllocatedObjectVisitor* visitor) {
#if defined(USE_TCMALLOC) && !defined(OS_WIN)
  IterateAllocatedObjects(&AllocationVisitor, visitor);
#endif
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

void DevToolsAgent::OnAddMessageToConsole(ConsoleMessageLevel level,
                                          const std::string& message) {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return;

  WebFrame* main_frame = web_view->mainFrame();
  if (!main_frame)
    return;

  WebConsoleMessage::Level target_level = WebConsoleMessage::LevelLog;
  switch (level) {
    case CONSOLE_MESSAGE_LEVEL_DEBUG:
      target_level = WebConsoleMessage::LevelDebug;
      break;
    case CONSOLE_MESSAGE_LEVEL_LOG:
      target_level = WebConsoleMessage::LevelLog;
      break;
    case CONSOLE_MESSAGE_LEVEL_WARNING:
      target_level = WebConsoleMessage::LevelWarning;
      break;
    case CONSOLE_MESSAGE_LEVEL_ERROR:
      target_level = WebConsoleMessage::LevelError;
      break;
  }
  main_frame->addMessageToConsole(
      WebConsoleMessage(target_level, WebString::fromUTF8(message)));
}

void DevToolsAgent::ContinueProgram() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  // TODO(pfeldman): rename didNavigate to continueProgram upstream.
  // That is in fact the purpose of the signal.
  if (web_agent)
    web_agent->didNavigate();
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

}  // namespace content
