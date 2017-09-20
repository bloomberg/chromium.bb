// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_agent.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/devtools_messages.h"
#include "content/common/frame_messages.h"
#include "content/public/common/manifest.h"
#include "content/renderer/devtools/devtools_client.h"
#include "content/renderer/devtools/devtools_cpu_throttler.h"
#include "content/renderer/manifest/manifest_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDevToolsAgent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

using blink::WebDevToolsAgent;
using blink::WebDevToolsAgentClient;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebString;

using base::trace_event::TraceLog;

namespace content {

namespace {

const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;
const char kPageGetAppManifest[] = "Page.getAppManifest";

class WebKitClientMessageLoopImpl
    : public WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() = default;
  ~WebKitClientMessageLoopImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop* const previous_run_loop = run_loop_;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;

    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    run_loop.Run();

    run_loop_ = previous_run_loop;
  }
  void QuitNow() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(run_loop_);

    run_loop_->Quit();
  }

 private:
  base::RunLoop* run_loop_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

typedef std::map<int, DevToolsAgent*> IdToAgentMap;
base::LazyInstance<IdToAgentMap>::Leaky
    g_agent_for_routing_id = LAZY_INSTANCE_INITIALIZER;

} //  namespace

DevToolsAgent::DevToolsAgent(RenderFrameImpl* frame)
    : RenderFrameObserver(frame),
      is_devtools_client_(false),
      paused_(false),
      frame_(frame),
      weak_factory_(this) {
  g_agent_for_routing_id.Get()[routing_id()] = this;
  frame_->GetWebFrame()->SetDevToolsAgentClient(this);
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
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_RequestNewWindow_ACK,
                        OnRequestNewWindowACK)
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

void DevToolsAgent::OnDestruct() {
  delete this;
}

void DevToolsAgent::SendProtocolMessage(int session_id,
                                        int call_id,
                                        const blink::WebString& message,
                                        const blink::WebString& state_cookie) {
  if (!send_protocol_message_callback_for_test_.is_null()) {
    send_protocol_message_callback_for_test_.Run(
        session_id, call_id, message.Utf8(), state_cookie.Utf8());
    return;
  }
  SendChunkedProtocolMessage(this, routing_id(), session_id, call_id,
                             message.Utf8(), state_cookie.Utf8());
}

// static
blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
DevToolsAgent::createMessageLoopWrapper() {
  return new WebKitClientMessageLoopImpl();
}

blink::WebDevToolsAgentClient::WebKitClientMessageLoop*
DevToolsAgent::CreateClientMessageLoop() {
  return createMessageLoopWrapper();
}

void DevToolsAgent::WillEnterDebugLoop() {
  paused_ = true;
}

void DevToolsAgent::DidExitDebugLoop() {
  paused_ = false;
}

bool DevToolsAgent::RequestDevToolsForFrame(blink::WebLocalFrame* webFrame) {
  RenderFrameImpl* frame = RenderFrameImpl::FromWebFrame(webFrame);
  if (!frame)
    return false;
  Send(new DevToolsAgentHostMsg_RequestNewWindow(routing_id(),
      frame->GetRoutingID()));
  return true;
}

void DevToolsAgent::EnableTracing(const WebString& category_filter) {
  // Tracing is already started by DevTools TracingHandler::Start for the
  // renderer target in the browser process. It will eventually start tracing in
  // the renderer process via IPC. But we still need a redundant
  // TraceLog::SetEnabled call here for
  // InspectorTracingAgent::emitMetadataEvents(), at which point, we are not
  // sure if tracing is already started in the renderer process.
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(
      base::trace_event::TraceConfig(category_filter.Utf8(), ""),
      TraceLog::RECORDING_MODE);
}

void DevToolsAgent::DisableTracing() {
  TraceLog::GetInstance()->SetDisabled();
}

void DevToolsAgent::SetCPUThrottlingRate(double rate) {
  DevToolsCPUThrottler::GetInstance()->SetThrottlingRate(rate);
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
    chunk.session_id = session_id;
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
  GetWebAgent()->Attach(WebString::FromUTF8(host_id), session_id);
  session_ids_.insert(session_id);
}

void DevToolsAgent::OnReattach(const std::string& host_id,
                               int session_id,
                               const std::string& agent_state) {
  GetWebAgent()->Reattach(WebString::FromUTF8(host_id), session_id,
                          WebString::FromUTF8(agent_state));
  session_ids_.insert(session_id);
}

void DevToolsAgent::OnDetach(int session_id) {
  GetWebAgent()->Detach(session_id);
  session_ids_.erase(session_id);
}

void DevToolsAgent::OnDispatchOnInspectorBackend(int session_id,
                                                 int call_id,
                                                 const std::string& method,
                                                 const std::string& message) {
  TRACE_EVENT0("devtools", "DevToolsAgent::OnDispatchOnInspectorBackend");
  if (method == kPageGetAppManifest) {
    ManifestManager* manager = frame_->manifest_manager();
    manager->GetManifest(base::BindOnce(&DevToolsAgent::GotManifest,
                                        weak_factory_.GetWeakPtr(), session_id,
                                        call_id));
    return;
  }
  GetWebAgent()->DispatchOnInspectorBackend(session_id, call_id,
                                            WebString::FromUTF8(method),
                                            WebString::FromUTF8(message));
}

void DevToolsAgent::OnInspectElement(int session_id, int x, int y) {
  blink::WebFloatRect point_rect(x, y, 0, 0);
  frame_->GetRenderWidget()->ConvertWindowToViewport(&point_rect);
  GetWebAgent()->InspectElementAt(session_id,
                                  WebPoint(point_rect.x, point_rect.y));
}

void DevToolsAgent::OnRequestNewWindowACK(bool success) {
  if (!success)
    GetWebAgent()->FailedToRequestDevTools();
}

void DevToolsAgent::ContinueProgram() {
  GetWebAgent()->ContinueProgram();
}

void DevToolsAgent::OnSetupDevToolsClient(const std::string& api_script) {
  // We only want to register once; and only in main frame.
  if (is_devtools_client_)
    return;
  is_devtools_client_ = true;
  new DevToolsClient(frame_, api_script);
}

WebDevToolsAgent* DevToolsAgent::GetWebAgent() {
  return frame_->GetWebFrame()->DevToolsAgent();
}

bool DevToolsAgent::IsAttached() {
  return !!session_ids_.size();
}

void DevToolsAgent::DetachAllSessions() {
  for (int session_id : session_ids_)
    GetWebAgent()->Detach(session_id);
  session_ids_.clear();
}

void DevToolsAgent::GotManifest(int session_id,
                                int call_id,
                                const GURL& manifest_url,
                                const Manifest& manifest,
                                const ManifestDebugInfo& debug_info) {
  if (!IsAttached())
    return;

  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->SetInteger("id", call_id);
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  std::unique_ptr<base::ListValue> errors(new base::ListValue());

  bool failed = false;
  for (const auto& error : debug_info.errors) {
    std::unique_ptr<base::DictionaryValue> error_value(
        new base::DictionaryValue());
    error_value->SetString("message", error.message);
    error_value->SetBoolean("critical", error.critical);
    error_value->SetInteger("line", error.line);
    error_value->SetInteger("column", error.column);
    if (error.critical)
      failed = true;
    errors->Append(std::move(error_value));
  }

  WebString url =
      frame_->GetWebFrame()->GetDocument().ManifestURL().GetString();
  result->SetString("url", url.Utf16());
  if (!failed)
    result->SetString("data", debug_info.raw_data);
  result->Set("errors", std::move(errors));
  response->Set("result", std::move(result));

  std::string json_message;
  base::JSONWriter::Write(*response, &json_message);
  SendChunkedProtocolMessage(this, routing_id(), session_id, call_id,
                             json_message, std::string());
}

}  // namespace content
