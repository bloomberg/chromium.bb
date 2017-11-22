// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

// static
bool DevToolsSession::ShouldSendOnIO(const std::string& method) {
  // Keep in sync with WebDevToolsAgent::ShouldInterruptForMethod.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics";
}

DevToolsSession::DevToolsSession(DevToolsAgentHostImpl* agent_host,
                                 DevToolsAgentHostClient* client,
                                 int session_id)
    : binding_(this),
      agent_host_(agent_host),
      client_(client),
      session_id_(session_id),
      process_(nullptr),
      host_(nullptr),
      dispatcher_(new protocol::UberDispatcher(this)),
      chunk_processor_(base::Bind(&DevToolsSession::SendMessageFromProcessorIPC,
                                  base::Unretained(this)),
                       base::Bind(&DevToolsSession::SendMessageFromProcessor,
                                  base::Unretained(this))),
      weak_factory_(this) {}

DevToolsSession::~DevToolsSession() {
  dispatcher_.reset();
  for (auto& pair : handlers_)
    pair.second->Disable();
  handlers_.clear();
}

void DevToolsSession::AddHandler(
    std::unique_ptr<protocol::DevToolsDomainHandler> handler) {
  handler->Wire(dispatcher_.get());
  handler->SetRenderer(process_, host_);
  handlers_[handler->name()] = std::move(handler);
}

void DevToolsSession::SetRenderFrameHost(RenderFrameHostImpl* frame_host) {
  SetRenderer(frame_host ? frame_host->GetProcess() : nullptr, frame_host);
}

void DevToolsSession::SetRenderer(RenderProcessHost* process_host,
                                  RenderFrameHostImpl* frame_host) {
  process_ = process_host;
  host_ = frame_host;
  for (auto& pair : handlers_)
    pair.second->SetRenderer(process_, host_);
}

void DevToolsSession::SetFallThroughForNotFound(bool value) {
  dispatcher_->setFallThroughForNotFound(value);
}

void DevToolsSession::AttachToAgent(
    const mojom::DevToolsAgentAssociatedPtr& agent) {
  mojom::DevToolsSessionHostAssociatedPtrInfo host_ptr_info;
  binding_.Bind(mojo::MakeRequest(&host_ptr_info));
  agent->AttachDevToolsSession(
      std::move(host_ptr_info), mojo::MakeRequest(&session_ptr_),
      mojo::MakeRequest(&io_session_ptr_), base::Optional<std::string>());
  session_ptr_.set_connection_error_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));
}

void DevToolsSession::ReattachToAgent(
    const mojom::DevToolsAgentAssociatedPtr& agent) {
  mojom::DevToolsSessionHostAssociatedPtrInfo host_ptr_info;
  binding_.Bind(mojo::MakeRequest(&host_ptr_info));
  agent->AttachDevToolsSession(
      std::move(host_ptr_info), mojo::MakeRequest(&session_ptr_),
      mojo::MakeRequest(&io_session_ptr_), state_cookie());
  session_ptr_.set_connection_error_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));
}

void DevToolsSession::SendMessageToClient(const std::string& message) {
  client_->DispatchProtocolMessage(agent_host_, message);
}

void DevToolsSession::SendMessageFromProcessorIPC(int session_id,
                                                  const std::string& message) {
  if (session_id != session_id_)
    return;
  int id = chunk_processor_.last_call_id();
  waiting_for_response_messages_.erase(id);
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
}

void DevToolsSession::SendMessageFromProcessor(const std::string& message) {
  int id = chunk_processor_.last_call_id();
  waiting_for_response_messages_.erase(id);
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
}

void DevToolsSession::SendResponse(
    std::unique_ptr<base::DictionaryValue> response) {
  std::string json;
  base::JSONWriter::Write(*response.get(), &json);
  client_->DispatchProtocolMessage(agent_host_, json);
}

void DevToolsSession::MojoConnectionDestroyed() {
  binding_.Close();
  session_ptr_.reset();
  io_session_ptr_.reset();
}

protocol::Response::Status DevToolsSession::Dispatch(
    const std::string& message,
    int* call_id,
    std::string* method) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(message);

  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (value && value->IsType(base::Value::Type::DICTIONARY) && delegate) {
    base::DictionaryValue* dict_value =
        static_cast<base::DictionaryValue*>(value.get());

    if (delegate->HandleCommand(agent_host_, session_id_, dict_value))
      return protocol::Response::kSuccess;

    if (delegate->HandleAsyncCommand(agent_host_, session_id_, dict_value,
                                     base::Bind(&DevToolsSession::SendResponse,
                                                weak_factory_.GetWeakPtr()))) {
      return protocol::Response::kAsync;
    }
  }

  return dispatcher_->dispatch(protocol::toProtocolValue(value.get(), 1000),
                               call_id, method);
}

void DevToolsSession::DispatchProtocolMessageToAgent(
    int call_id,
    const std::string& method,
    const std::string& message) {
  if (ShouldSendOnIO(method)) {
    if (io_session_ptr_)
      io_session_ptr_->DispatchProtocolMessage(call_id, method, message);
  } else {
    if (session_ptr_)
      session_ptr_->DispatchProtocolMessage(call_id, method, message);
  }
}

void DevToolsSession::InspectElement(const gfx::Point& point) {
  if (session_ptr_)
    session_ptr_->InspectElement(point);
}

bool DevToolsSession::ReceiveMessageChunk(const DevToolsMessageChunk& chunk) {
  return chunk_processor_.ProcessChunkedMessageFromAgent(chunk);
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  client_->DispatchProtocolMessage(agent_host_, message->serialize());
}

void DevToolsSession::flushProtocolNotifications() {
}

void DevToolsSession::DispatchProtocolMessage(
    mojom::DevToolsMessageChunkPtr chunk) {
  if (chunk_processor_.ProcessChunkedMessageFromAgent(std::move(chunk)))
    return;

  MojoConnectionDestroyed();
  if (process_) {
    bad_message::ReceivedBadMessage(
        process_, bad_message::RFH_INCONSISTENT_DEVTOOLS_MESSAGE);
  }
}

void DevToolsSession::RequestNewWindow(int32_t frame_routing_id,
                                       RequestNewWindowCallback callback) {
  bool success = false;
  RenderFrameHostImpl* frame_host =
      process_
          ? RenderFrameHostImpl::FromID(process_->GetID(), frame_routing_id)
          : nullptr;
  if (frame_host) {
    scoped_refptr<DevToolsAgentHost> agent =
        RenderFrameDevToolsAgentHost::GetOrCreateFor(
            frame_host->frame_tree_node());
    success = static_cast<DevToolsAgentHostImpl*>(agent.get())->Inspect();
  }
  std::move(callback).Run(success);
}

}  // namespace content
