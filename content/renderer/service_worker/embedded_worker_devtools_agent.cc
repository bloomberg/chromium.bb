// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/embedded_worker_devtools_agent.h"

#include "content/common/devtools_messages.h"
#include "content/renderer/devtools/devtools_agent.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebEmbeddedWorker.h"

using blink::WebEmbeddedWorker;
using blink::WebString;

namespace content {

namespace {
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;
}

EmbeddedWorkerDevToolsAgent::EmbeddedWorkerDevToolsAgent(
    blink::WebEmbeddedWorker* webworker,
    int route_id)
    : webworker_(webworker), route_id_(route_id) {
  RenderThreadImpl::current()->AddRoute(route_id_, this);
}

EmbeddedWorkerDevToolsAgent::~EmbeddedWorkerDevToolsAgent() {
  RenderThreadImpl::current()->RemoveRoute(route_id_);
}

void EmbeddedWorkerDevToolsAgent::SendMessage(IPC::Sender* sender,
                                              int session_id,
                                              int call_id,
                                              std::string message,
                                              std::string state_cookie) {
  bool single_chunk = message.length() < kMaxMessageChunkSize;
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    DevToolsMessageChunk chunk;
    chunk.is_first = pos == 0;
    chunk.message_size = pos == 0 ? message.size() : 0;
    chunk.is_last = pos + kMaxMessageChunkSize >= message.length();
    chunk.session_id = session_id;
    chunk.call_id = chunk.is_last ? call_id : 0;
    chunk.post_state = chunk.is_last ? std::move(state_cookie) : std::string();
    chunk.data = single_chunk ? std::move(message)
                              : message.substr(pos, kMaxMessageChunkSize);
    sender->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_, std::move(chunk)));
  }
}

bool EmbeddedWorkerDevToolsAgent::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDevToolsAgent, message)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
  IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                      OnDispatchOnInspectorBackend)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDevToolsAgent::OnAttach(int session_id) {
  webworker_->AttachDevTools(session_id);
}

void EmbeddedWorkerDevToolsAgent::OnReattach(int session_id,
                                             const std::string& state) {
  webworker_->ReattachDevTools(session_id, WebString::FromUTF8(state));
}

void EmbeddedWorkerDevToolsAgent::OnDetach(int session_id) {
  webworker_->DetachDevTools(session_id);
}

void EmbeddedWorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    int session_id,
    int call_id,
    const std::string& method,
    const std::string& message) {
  webworker_->DispatchDevToolsMessage(session_id, call_id,
                                      WebString::FromUTF8(method),
                                      WebString::FromUTF8(message));
}

}  // namespace content
