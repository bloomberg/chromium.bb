// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_client_impl.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_event_listener.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"

namespace {

const char* kInspectorContextError =
    "Execution context with given id not found.";

Status ParseInspectorError(const std::string& error_json) {
  scoped_ptr<base::Value> error(base::JSONReader::Read(error_json));
  base::DictionaryValue* error_dict;
  if (!error || !error->GetAsDictionary(&error_dict))
    return Status(kUnknownError, "inspector error with no error message");
  std::string error_message;
  if (error_dict->GetString("message", &error_message) &&
      error_message == kInspectorContextError) {
    return Status(kNoSuchFrame);
  }
  return Status(kUnknownError, "unhandled inspector error: " + error_json);
}

}  // namespace

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

DevToolsClientImpl::~DevToolsClientImpl() {
  for (ResponseMap::iterator iter = cmd_response_map_.begin();
       iter != cmd_response_map_.end(); ++iter) {
    LOG(WARNING) << "Finished with no response for command " << iter->first;
    delete iter->second;
  }
}

void DevToolsClientImpl::SetParserFuncForTesting(
    const ParserFunc& parser_func) {
  parser_func_ = parser_func;
}

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

Status DevToolsClientImpl::HandleEventsUntil(
    const ConditionalFunc& conditional_func) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;

  while (socket_->HasNextMessage() || !conditional_func.Run()) {
    Status status = ReceiveNextMessage(-1, &type, &event, &response);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  if (!connected_) {
    if (!socket_->Connect(url_))
      return Status(kDisconnected, "unable to connect to renderer");
    connected_ = true;

    // OnConnected notification will be sent out in method ReceiveNextMessage.
    listeners_for_on_connected_ = listeners_;
  }

  int command_id = next_id_++;
  base::DictionaryValue command;
  command.SetInteger("id", command_id);
  command.SetString("method", method);
  command.Set("params", params.DeepCopy());
  std::string message;
  base::JSONWriter::Write(&command, &message);
  if (!socket_->Send(message)) {
    connected_ = false;
    return Status(kDisconnected, "unable to send message to renderer");
  }
  return ReceiveCommandResponse(command_id, result);
}

Status DevToolsClientImpl::ReceiveCommandResponse(
    int command_id,
    scoped_ptr<base::DictionaryValue>* result) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  cmd_response_map_[command_id] = NULL;
  while (!HasReceivedCommandResponse(command_id)) {
    Status status = ReceiveNextMessage(command_id, &type, &event, &response);
    if (status.IsError())
      return status;
  }
  result->reset(cmd_response_map_[command_id]);
  cmd_response_map_.erase(command_id);
  return Status(kOk);
}

Status DevToolsClientImpl::ReceiveNextMessage(
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* response) {
  while (!listeners_for_on_connected_.empty()) {
    DevToolsEventListener* listener = listeners_for_on_connected_.front();
    listeners_for_on_connected_.pop_front();
    Status status = listener->OnConnected();
    if (status.IsError())
      return status;
  }
  // The message might be received already when processing other commands sent
  // from DevToolsEventListener::OnConnected.
  if (HasReceivedCommandResponse(expected_id))
    return Status(kOk);

  std::string message;
  if (!socket_->ReceiveNextMessage(&message)) {
    connected_ = false;
    return Status(kDisconnected,
                  "unable to receive message from renderer");
  }
  if (!parser_func_.Run(message, expected_id, type, event, response))
    return Status(kUnknownError, "bad inspector message: " + message);
  if (*type == internal::kEventMessageType)
    return NotifyEventListeners(event->method, *event->params);
  if (*type == internal::kCommandResponseMessageType) {
    if (cmd_response_map_.count(response->id) == 0) {
      return Status(kUnknownError, "unexpected command message");
    } else if (response->result) {
      cmd_response_map_[response->id] = response->result.release();
    } else {
      cmd_response_map_.erase(response->id);
      return ParseInspectorError(response->error);
    }
  }
  return Status(kOk);
}

bool DevToolsClientImpl::HasReceivedCommandResponse(int cmd_id) {
  return cmd_response_map_.find(cmd_id) != cmd_response_map_.end()
      && cmd_response_map_[cmd_id] != NULL;
}

Status DevToolsClientImpl::NotifyEventListeners(
    const std::string& method,
    const base::DictionaryValue& params) {
  for (std::list<DevToolsEventListener*>::iterator iter = listeners_.begin();
       iter != listeners_.end(); ++iter) {
    (*iter)->OnEvent(method, params);
  }
  if (method == "Inspector.detached") {
    connected_ = false;
    return Status(kDisconnected, "received Inspector.detached event");
  }
  return Status(kOk);
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
