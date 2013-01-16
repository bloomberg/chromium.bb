// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_client_impl.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_event_listener.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"

namespace internal {

InspectorEvent::InspectorEvent() {}

InspectorEvent::~InspectorEvent() {}

InspectorCommandResponse::InspectorCommandResponse() {}

InspectorCommandResponse::~InspectorCommandResponse() {}

}  // namespace internal

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url)
    : socket_(factory.Run().Pass()),
      url_(url),
      parser_func_(base::Bind(&internal::ParseInspectorMessage)),
      connected_(false),
      next_id_(1) {}

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url,
    const ParserFunc& parser_func)
    : socket_(factory.Run().Pass()),
      url_(url),
      parser_func_(parser_func),
      connected_(false),
      next_id_(1) {}

DevToolsClientImpl::~DevToolsClientImpl() {}

Status DevToolsClientImpl::SendCommand(
    const std::string& method,
    const base::DictionaryValue& params) {
  scoped_ptr<base::DictionaryValue> result;
  return SendCommandInternal(method, params, &result);
}

Status DevToolsClientImpl::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  scoped_ptr<base::DictionaryValue> intermediate_result;
  Status status = SendCommandInternal(method, params, &intermediate_result);
  if (status.IsError())
    return status;
  if (!intermediate_result)
    return Status(kUnknownError, "inspector response missing result");
  result->reset(intermediate_result.release());
  return Status(kOk);
}

void DevToolsClientImpl::AddListener(DevToolsEventListener* listener) {
  DCHECK(listener);
  listeners_.push_back(listener);
}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  if (!connected_) {
    if (!socket_->Connect(url_))
      return Status(kUnknownError, "unable to connect to renderer");
    connected_ = true;
  }

  int command_id = next_id_++;
  base::DictionaryValue command;
  command.SetInteger("id", command_id);
  command.SetString("method", method);
  command.Set("params", params.DeepCopy());
  std::string message;
  base::JSONWriter::Write(&command, &message);
  if (!socket_->Send(message))
    return Status(kUnknownError, "unable to send message to renderer");

  scoped_ptr<base::DictionaryValue> response_dict;
  while (true) {
    std::string message;
    if (!socket_->ReceiveNextMessage(&message))
      return Status(kUnknownError, "unable to receive message from renderer");
    internal::InspectorMessageType type;
    internal::InspectorEvent event;
    internal::InspectorCommandResponse response;
    if (!parser_func_.Run(message, command_id, &type, &event, &response))
      return Status(kUnknownError, "bad inspector message: " + message);
    if (type == internal::kEventMessageType) {
      NotifyEventListeners(event.method, *event.params);
    } else {
      if (response.id != command_id) {
        return Status(kUnknownError,
                      "received response for unknown command ID");
      }
      if (response.result) {
        result->reset(response.result.release());
        return Status(kOk);
      }
      return Status(kUnknownError, "inspector error: " + response.error);
    }
  }
}

void DevToolsClientImpl::NotifyEventListeners(
    const std::string& method,
    const base::DictionaryValue& params) {
  for (std::list<DevToolsEventListener*>::iterator iter = listeners_.begin();
       iter != listeners_.end(); ++iter) {
    (*iter)->OnEvent(method, params);
  }
}

namespace internal {

bool ParseInspectorMessage(
    const std::string& message,
    int expected_id,
    InspectorMessageType* type,
    InspectorEvent* event,
    InspectorCommandResponse* command_response) {
  scoped_ptr<base::Value> message_value(base::JSONReader::Read(message));
  base::DictionaryValue* message_dict;
  if (!message_value || !message_value->GetAsDictionary(&message_dict))
    return false;

  int id;
  if (!message_dict->HasKey("id")) {
    std::string method;
    if (!message_dict->GetString("method", &method))
      return false;
    base::DictionaryValue* params = NULL;
    message_dict->GetDictionary("params", &params);

    *type = kEventMessageType;
    event->method = method;
    if (params)
      event->params.reset(params->DeepCopy());
    else
      event->params.reset(new base::DictionaryValue());
    return true;
  } else if (message_dict->GetInteger("id", &id)) {
    base::DictionaryValue* unscoped_error = NULL;
    base::DictionaryValue* unscoped_result = NULL;
    if (!message_dict->GetDictionary("error", &unscoped_error) &&
        !message_dict->GetDictionary("result", &unscoped_result))
      return false;

    *type = kCommandResponseMessageType;
    command_response->id = id;
    if (unscoped_result)
      command_response->result.reset(unscoped_result->DeepCopy());
    else
      base::JSONWriter::Write(unscoped_error, &command_response->error);
    return true;
  }
  return false;
}

}  // namespace internal
