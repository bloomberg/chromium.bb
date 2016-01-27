// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent.h"

#include <stddef.h>

#include <map>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/devtools_messages.h"
#include "content/common/frame_messages.h"
#include "content/renderer/devtools/devtools_client.h"
#include "content/renderer/devtools/devtools_cpu_throttler.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebConsoleMessage;
using blink::WebDevToolsAgent;
using blink::WebDevToolsAgentClient;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebString;

using base::trace_event::TraceLog;

namespace content {

namespace {

const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(base::MessageLoop::current()) {}
  ~WebKitClientMessageLoopImpl() override { message_loop_ = NULL; }
  void run() override {
    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop_);
    message_loop_->Run();
  }
  void quitNow() override { message_loop_->QuitNow(); }

 private:
  base::MessageLoop* message_loop_;
};

typedef std::map<int, DevToolsAgent*> IdToAgentMap;
base::LazyInstance<IdToAgentMap>::Leaky
    g_agent_for_routing_id = LAZY_INSTANCE_INITIALIZER;

} //  namespace

DevToolsAgent::DevToolsAgent(RenderFrameImpl* frame)
    : RenderFrameObserver(frame),
      is_attached_(false),
      is_devtools_client_(false),
      paused_in_mouse_move_(false),
      paused_(false),
      frame_(frame),
      cpu_throttler_(new DevToolsCPUThrottler()) {
  g_agent_for_routing_id.Get()[routing_id()] = this;
  frame_->GetWebFrame()->setDevToolsAgentClient(this);
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

  if (message.type() == FrameMsg_Navigate::ID)
    ContinueProgram();  // Don't want to swallow the message.

  return handled;
}

void DevToolsAgent::WidgetWillClose() {
  ContinueProgram();
}

void DevToolsAgent::sendProtocolMessage(int session_id,
                                        int call_id,
                                        const blink::WebString& message,
                                        const blink::WebString& state_cookie) {
  SendChunkedProtocolMessage(this, routing_id(), session_id, call_id,
                             message.utf8(), state_cookie.utf8());
}

blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
    DevToolsAgent::createClientMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void DevToolsAgent::willEnterDebugLoop() {
  paused_ = true;
  if (RenderWidget* widget = frame_->GetRenderWidget())
    paused_in_mouse_move_ = widget->SendAckForMouseMoveFromDebugger();
}

void DevToolsAgent::didExitDebugLoop() {
  paused_ = false;
  if (!paused_in_mouse_move_)
    return;
  if (RenderWidget* widget = frame_->GetRenderWidget()) {
    widget->IgnoreAckForMouseMoveFromDebugger();
    paused_in_mouse_move_ = false;
  }
}

void DevToolsAgent::enableTracing(const WebString& category_filter) {
  // Tracing is already started by DevTools TracingHandler::Start for the
  // renderer target in the browser process. It will eventually start tracing in
  // the renderer process via IPC. But we still need a redundant
  // TraceLog::SetEnabled call here for
  // InspectorTracingAgent::emitMetadataEvents(), at which point, we are not
  // sure if tracing is already started in the renderer process.
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(
      base::trace_event::TraceConfig(category_filter.utf8(), ""),
      TraceLog::RECORDING_MODE);
}

void DevToolsAgent::disableTracing() {
  TraceLog::GetInstance()->SetDisabled();
}

void DevToolsAgent::setCPUThrottlingRate(double rate) {
  cpu_throttler_->SetThrottlingRate(rate);
}

// static
DevToolsAgent* DevToolsAgent::FromRoutingId(int routing_id) {
  IdToAgentMap::iterator it = g_agent_for_routing_id.Get().find(routing_id);
  if (it != g_agent_for_routing_id.Get().end()) {
    return it->second;
  }
  return NULL;
}

// static
void DevToolsAgent::SendChunkedProtocolMessage(IPC::Sender* sender,
                                               int routing_id,
                                               int session_id,
                                               int call_id,
                                               const std::string& message,
                                               const std::string& post_state) {
  DevToolsMessageChunk chunk;
  chunk.message_size = message.size();
  chunk.is_first = true;

  if (message.length() < kMaxMessageChunkSize) {
    chunk.data = message;
    chunk.session_id = session_id;
    chunk.call_id = call_id;
    chunk.post_state = post_state;
    chunk.is_last = true;
    sender->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
                     routing_id, chunk));
    return;
  }

  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    chunk.is_last = pos + kMaxMessageChunkSize >= message.length();
    chunk.session_id = chunk.is_last ? session_id : 0;
    chunk.call_id = chunk.is_last ? call_id : 0;
    chunk.post_state = chunk.is_last ? post_state : std::string();
    chunk.data = message.substr(pos, kMaxMessageChunkSize);
    sender->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
                     routing_id, chunk));
    chunk.is_first = false;
    chunk.message_size = 0;
  }
}

void DevToolsAgent::OnAttach(const std::string& host_id, int session_id) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->attach(WebString::fromUTF8(host_id), session_id);
    is_attached_ = true;
  }
}

void DevToolsAgent::OnReattach(const std::string& host_id,
                               int session_id,
                               const std::string& agent_state) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    web_agent->reattach(WebString::fromUTF8(host_id), session_id,
                        WebString::fromUTF8(agent_state));
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

void DevToolsAgent::OnDispatchOnInspectorBackend(int session_id,
                                                 const std::string& message) {
  TRACE_EVENT0("devtools", "DevToolsAgent::OnDispatchOnInspectorBackend");
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->dispatchOnInspectorBackend(session_id,
                                          WebString::fromUTF8(message));
}

void DevToolsAgent::OnInspectElement(int x, int y) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent) {
    DCHECK(is_attached_);
    web_agent->inspectElementAt(WebPoint(x, y));
  }
}

void DevToolsAgent::AddMessageToConsole(ConsoleMessageLevel level,
                                        const std::string& message) {
  WebLocalFrame* web_frame = frame_->GetWebFrame();
  if (!web_frame)
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
  web_frame->addMessageToConsole(
      WebConsoleMessage(target_level, WebString::fromUTF8(message)));
}

void DevToolsAgent::ContinueProgram() {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->continueProgram();
}

void DevToolsAgent::OnSetupDevToolsClient(
    const std::string& compatibility_script) {
  // We only want to register once; and only in main frame.
  DCHECK(!frame_->GetWebFrame() || !frame_->GetWebFrame()->parent());
  if (is_devtools_client_)
    return;
  is_devtools_client_ = true;
  new DevToolsClient(frame_, compatibility_script);
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  WebLocalFrame* web_frame = frame_->GetWebFrame();
  return web_frame ? web_frame->devToolsAgent() : nullptr;
}

bool DevToolsAgent::IsAttached() {
  return is_attached_;
}

}  // namespace content
