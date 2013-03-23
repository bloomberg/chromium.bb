// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

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

class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(int* count) : count_(count) {
    (*count_)++;
  }
  ~ScopedIncrementer() {
    (*count_)--;
  }

 private:
  int* count_;
};

}  // namespace

namespace internal {

InspectorEvent::InspectorEvent() {}

InspectorEvent::~InspectorEvent() {}

InspectorCommandResponse::InspectorCommandResponse() {}

InspectorCommandResponse::~InspectorCommandResponse() {}

}  // namespace internal

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url,
    const FrontendCloserFunc& frontend_closer_func)
    : socket_(factory.Run().Pass()),
      url_(url),
      frontend_closer_func_(frontend_closer_func),
      parser_func_(base::Bind(&internal::ParseInspectorMessage)),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {}

DevToolsClientImpl::DevToolsClientImpl(
    const SyncWebSocketFactory& factory,
    const std::string& url,
    const FrontendCloserFunc& frontend_closer_func,
    const ParserFunc& parser_func)
    : socket_(factory.Run().Pass()),
      url_(url),
      frontend_closer_func_(frontend_closer_func),
      parser_func_(parser_func),
      unnotified_event_(NULL),
      next_id_(1),
      stack_count_(0) {}

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

Status DevToolsClientImpl::ConnectIfNecessary() {
  if (stack_count_)
    return Status(kUnknownError, "cannot connect when nested");

  if (socket_->IsConnected())
    return Status(kOk);

  if (!socket_->Connect(url_)) {
    // Try to close devtools frontend and then reconnect.
    Status status = frontend_closer_func_.Run();
    if (status.IsError())
      return status;
    if (!socket_->Connect(url_))
      return Status(kDisconnected, "unable to connect to renderer");
  }

  unnotified_connect_listeners_ = listeners_;
  // Notify all listeners of the new connection. Do this now so that any errors
  // that occur are reported now instead of later during some unrelated call.
  // Also gives listeners a chance to send commands before other clients.
  return EnsureListenersNotifiedOfConnect();
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
  if (!socket_->IsConnected())
    return Status(kDisconnected, "not connected to DevTools");

  while (true) {
    if (!socket_->HasNextMessage()) {
      bool is_condition_met;
      Status status = conditional_func.Run(&is_condition_met);
      if (status.IsError())
        return status;
      if (is_condition_met)
        return Status(kOk);
    }

    Status status = ReceiveNextMessage(-1);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::SendCommandInternal(
    const std::string& method,
    const base::DictionaryValue& params,
    scoped_ptr<base::DictionaryValue>* result) {
  if (!socket_->IsConnected())
    return Status(kDisconnected, "not connected to DevTools");

  int command_id = next_id_++;
  base::DictionaryValue command;
  command.SetInteger("id", command_id);
  command.SetString("method", method);
  command.Set("params", params.DeepCopy());
  std::string message;
  base::JSONWriter::Write(&command, &message);
  if (!socket_->Send(message))
    return Status(kDisconnected, "unable to send message to renderer");

  cmd_response_map_[command_id] = NULL;
  while (!HasReceivedCommandResponse(command_id)) {
    Status status = ReceiveNextMessage(command_id);
    if (status.IsError())
      return status;
  }
  result->reset(cmd_response_map_[command_id]);
  cmd_response_map_.erase(command_id);
  return Status(kOk);
}

Status DevToolsClientImpl::ReceiveNextMessage(int expected_id) {
  ScopedIncrementer increment_stack_count(&stack_count_);

  Status status = EnsureListenersNotifiedOfConnect();
  if (status.IsError())
    return status;
  status = EnsureListenersNotifiedOfEvent();
  if (status.IsError())
    return status;

  // The command response may have already been received while notifying
  // listeners.
  if (HasReceivedCommandResponse(expected_id))
    return Status(kOk);

  std::string message;
  if (!socket_->ReceiveNextMessage(&message))
    return Status(kDisconnected, "unable to receive message from renderer");

  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  if (!parser_func_.Run(message, expected_id, &type, &event, &response))
    return Status(kUnknownError, "bad inspector message: " + message);
  if (type == internal::kEventMessageType) {
    unnotified_event_listeners_ = listeners_;
    unnotified_event_ = &event;
    Status status = EnsureListenersNotifiedOfEvent();
    unnotified_event_ = NULL;
    if (status.IsError())
      return status;
    if (event.method == "Inspector.detached")
      return Status(kDisconnected, "received Inspector.detached event");
  } else if (type == internal::kCommandResponseMessageType) {
    if (cmd_response_map_.count(response.id) == 0) {
      return Status(kUnknownError, "unexpected command message");
    } else if (response.result) {
      cmd_response_map_[response.id] = response.result.release();
    } else {
      cmd_response_map_.erase(response.id);
      return ParseInspectorError(response.error);
    }
  }
  return Status(kOk);
}

bool DevToolsClientImpl::HasReceivedCommandResponse(int cmd_id) {
  return cmd_response_map_.find(cmd_id) != cmd_response_map_.end()
      && cmd_response_map_[cmd_id] != NULL;
}

Status DevToolsClientImpl::EnsureListenersNotifiedOfConnect() {
  while (unnotified_connect_listeners_.size()) {
    DevToolsEventListener* listener = unnotified_connect_listeners_.front();
    unnotified_connect_listeners_.pop_front();
    Status status = listener->OnConnected();
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status DevToolsClientImpl::EnsureListenersNotifiedOfEvent() {
  while (unnotified_event_listeners_.size()) {
    DevToolsEventListener* listener = unnotified_event_listeners_.front();
    unnotified_event_listeners_.pop_front();
    listener->OnEvent(unnotified_event_->method, *unnotified_event_->params);
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
