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

DevToolsSession::DevToolsSession(
    DevToolsAgentHostImpl* agent_host,
    int session_id)
    : agent_host_(agent_host),
      session_id_(session_id),
      dispatcher_(new protocol::UberDispatcher(this)) {
}

DevToolsSession::~DevToolsSession() {}

void DevToolsSession::ResetDispatcher() {
  dispatcher_.reset();
}

protocol::Response::Status DevToolsSession::Dispatch(
    const std::string& message,
    int* call_id,
    std::string* method) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(message);

  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (value && value->IsType(base::Value::Type::DICTIONARY) && delegate) {
    std::unique_ptr<base::DictionaryValue> response(delegate->HandleCommand(
        agent_host_,
        static_cast<base::DictionaryValue*>(value.get())));
    if (response) {
      std::string json;
      base::JSONWriter::Write(*response.get(), &json);
      agent_host_->SendMessageToClient(session_id_, json);
      return protocol::Response::kSuccess;
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

}  // namespace content
