// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

DevToolsSession::DevToolsSession(DevToolsAgentHostImpl* agent_host,
                                 DevToolsAgentHostClient* client,
                                 int session_id)
    : agent_host_(agent_host),
      client_(client),
      session_id_(session_id),
      host_(nullptr),
      dispatcher_(new protocol::UberDispatcher(this)),
      chunk_processor_(base::Bind(&DevToolsSession::SendMessageFromProcessor,
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
  handler->SetRenderFrameHost(host_);
  handlers_[handler->name()] = std::move(handler);
}

void DevToolsSession::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
  for (auto& pair : handlers_)
    pair.second->SetRenderFrameHost(host_);
}

void DevToolsSession::SetFallThroughForNotFound(bool value) {
  dispatcher_->setFallThroughForNotFound(value);
}

void DevToolsSession::SendMessageToClient(const std::string& message) {
  client_->DispatchProtocolMessage(agent_host_, message);
}

void DevToolsSession::SendMessageFromProcessor(int session_id,
                                               const std::string& message) {
  if (session_id != session_id_)
    return;
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
    std::unique_ptr<base::DictionaryValue> response(
        delegate->HandleCommand(agent_host_, dict_value));
    if (response) {
      SendResponse(std::move(response));
      return protocol::Response::kSuccess;
    }
    if (delegate->HandleAsyncCommand(agent_host_, dict_value,
                                     base::Bind(&DevToolsSession::SendResponse,
                                                weak_factory_.GetWeakPtr()))) {
      return protocol::Response::kAsync;
    }
  }

  return dispatcher_->dispatch(protocol::toProtocolValue(value.get(), 1000),
                               call_id, method);
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

}  // namespace content
