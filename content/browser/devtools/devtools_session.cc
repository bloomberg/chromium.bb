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

namespace {

bool ShouldSendOnIO(const std::string& method) {
  // Keep in sync with WebDevToolsAgent::ShouldInterruptForMethod.
  // TODO(dgozman): find a way to share this.
  return method == "Debugger.pause" || method == "Debugger.setBreakpoint" ||
         method == "Debugger.setBreakpointByUrl" ||
         method == "Debugger.removeBreakpoint" ||
         method == "Debugger.setBreakpointsActive" ||
         method == "Performance.getMetrics";
}

}  // namespace

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
      mojo::MakeRequest(&io_session_ptr_), state_cookie_);
  session_ptr_.set_connection_error_handler(base::BindOnce(
      &DevToolsSession::MojoConnectionDestroyed, base::Unretained(this)));
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
  if (value && value->is_dict() && delegate) {
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

void DevToolsSession::ReceiveMessageChunk(const DevToolsMessageChunk& chunk) {
  if (chunk.session_id != session_id_)
    return;

  if (chunk.is_first) {
    if (response_message_buffer_size_ != 0)
      return;
    if (chunk.is_last) {
      response_message_buffer_size_ = chunk.data.size();
    } else {
      response_message_buffer_size_ = chunk.message_size;
      response_message_buffer_.reserve(chunk.message_size);
    }
  }

  if (response_message_buffer_.size() + chunk.data.size() >
      response_message_buffer_size_)
    return;
  response_message_buffer_.append(chunk.data);

  if (!chunk.is_last)
    return;
  if (response_message_buffer_.size() != response_message_buffer_size_)
    return;

  if (!chunk.post_state.empty())
    state_cookie_ = chunk.post_state;
  waiting_for_response_messages_.erase(chunk.call_id);
  response_message_buffer_size_ = 0;
  std::string message;
  message.swap(response_message_buffer_);
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
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
  if (chunk->is_first) {
    if (response_message_buffer_size_ != 0) {
      ReceivedBadMessage();
      return;
    }
    if (chunk->is_last) {
      response_message_buffer_size_ = chunk->data.size();
    } else {
      response_message_buffer_size_ = chunk->message_size;
      response_message_buffer_.reserve(chunk->message_size);
    }
  }

  if (response_message_buffer_.size() + chunk->data.size() >
      response_message_buffer_size_) {
    ReceivedBadMessage();
    return;
  }
  response_message_buffer_.append(std::move(chunk->data));

  if (!chunk->is_last)
    return;
  if (response_message_buffer_.size() != response_message_buffer_size_) {
    ReceivedBadMessage();
    return;
  }

  if (!chunk->post_state.empty())
    state_cookie_ = std::move(chunk->post_state);
  waiting_for_response_messages_.erase(chunk->call_id);
  response_message_buffer_size_ = 0;
  std::string message;
  message.swap(response_message_buffer_);
  client_->DispatchProtocolMessage(agent_host_, message);
  // |this| may be deleted at this point.
}

void DevToolsSession::ReceivedBadMessage() {
  MojoConnectionDestroyed();
  if (process_) {
    bad_message::ReceivedBadMessage(
        process_, bad_message::RFH_INCONSISTENT_DEVTOOLS_MESSAGE);
  }
}

}  // namespace content
