// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_session.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
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

void DevToolsSession::sendResponse(
    std::unique_ptr<base::DictionaryValue> response) {
  std::string json;
  base::JSONWriter::Write(*response.get(), &json);
  agent_host_->SendMessageToClient(session_id_, json);
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
      sendResponse(std::move(response));
      return protocol::Response::kSuccess;
    }
    if (delegate->HandleAsyncCommand(agent_host_, dict_value,
                                     base::Bind(&DevToolsSession::sendResponse,
                                                weak_factory_.GetWeakPtr()))) {
      return protocol::Response::kAsync;
    }
  }

  return dispatcher_->dispatch(protocol::toProtocolValue(value.get(), 1000),
                               call_id, method);
}

void DevToolsSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  agent_host_->SendMessageToClient(session_id_, message->serialize());
}

void DevToolsSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> message) {
  agent_host_->SendMessageToClient(session_id_, message->serialize());
}

void DevToolsSession::flushProtocolNotifications() {
}

protocol::DevToolsDomainHandler* DevToolsSession::GetHandlerByName(
    const std::string& name) {
  auto it = handlers_.find(name);
  if (it == handlers_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace content
