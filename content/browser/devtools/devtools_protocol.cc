// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stringprintf.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";
const char kResultParam[] = "result";
const char kErrorParam[] = "error";
const char kErrorCodeParam[] = "code";
const char kErrorMessageParam[] = "message";
const int kNoId = -1;

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
enum Error {
  kErrorParseError = -32700,
  kErrorInvalidRequest = -32600,
  kErrorNoSuchMethod = -32601,
  kErrorInvalidParams = -32602,
  kErrorInternalError = -32603
};

}  // namespace

using base::DictionaryValue;
using base::Value;

DevToolsProtocol::Message::~Message() {
}

DevToolsProtocol::Message::Message(const std::string& method,
                                   DictionaryValue* params)
    : method_(method),
      params_(params) {
  size_t pos = method.find(".");
  if (pos != std::string::npos && pos > 0)
    domain_ = method.substr(0, pos);
}

DevToolsProtocol::Command::~Command() {
}

std::string DevToolsProtocol::Command::Serialize() {
  DictionaryValue command;
  command.SetInteger(kIdParam, id_);
  command.SetString(kMethodParam, method_);
  if (params_)
    command.Set(kParamsParam, params_->DeepCopy());

  std::string json_command;
  base::JSONWriter::Write(&command, &json_command);
  return json_command;
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::SuccessResponse(DictionaryValue* result) {
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, result));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::InternalErrorResponse(const std::string& message) {
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, kErrorInternalError, message));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::InvalidParamResponse(const std::string& param) {
  std::string message =
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str());
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, kErrorInvalidParams, message));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::NoSuchMethodErrorResponse() {
  return scoped_ptr<DevToolsProtocol::Response>(
      new Response(id_, kErrorNoSuchMethod, "No such method"));
}

DevToolsProtocol::Command::Command(int id,
                                   const std::string& method,
                                   DictionaryValue* params)
    : Message(method, params),
      id_(id) {
}

std::string DevToolsProtocol::Response::Serialize() {
  DictionaryValue response;

  if (id_ != kNoId)
    response.SetInteger(kIdParam, id_);

  if (error_code_) {
    DictionaryValue* error_object = new DictionaryValue();
    response.Set(kErrorParam, error_object);
    error_object->SetInteger(kErrorCodeParam, error_code_);
    if (!error_message_.empty())
      error_object->SetString(kErrorMessageParam, error_message_);
  } else if (result_) {
    response.Set(kResultParam, result_->DeepCopy());
  }

  std::string json_response;
  base::JSONWriter::Write(&response, &json_response);
  return json_response;
}

DevToolsProtocol::Response::Response(int id, DictionaryValue* result)
    : id_(id),
      result_(result),
      error_code_(0) {
}

DevToolsProtocol::Response::Response(int id,
                                     int error_code,
                                     const std::string& error_message)
    : id_(id),
      result_(NULL),
      error_code_(error_code),
      error_message_(error_message) {
}

DevToolsProtocol::Response::~Response() {
}

DevToolsProtocol::Notification::Notification(const std::string& method,
                                             DictionaryValue* params)
    : Message(method, params) {
}

DevToolsProtocol::Notification::~Notification() {
}

std::string DevToolsProtocol::Notification::Serialize() {
  DictionaryValue notification;
  notification.SetString(kMethodParam, method_);
  if (params_)
    notification.Set(kParamsParam, params_->DeepCopy());

  std::string json_notification;
  base::JSONWriter::Write(&notification, &json_notification);
  return json_notification;
}

DevToolsProtocol::Handler::~Handler() {
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Handler::HandleCommand(
    DevToolsProtocol::Command* command) {
  CommandHandlers::iterator it = command_handlers_.find(command->method());
  if (it == command_handlers_.end())
    return scoped_ptr<DevToolsProtocol::Response>();
  return (it->second).Run(command);
}

void DevToolsProtocol::Handler::SetNotifier(const Notifier& notifier) {
  notifier_ = notifier;
}

DevToolsProtocol::Handler::Handler() {
}

void DevToolsProtocol::Handler::RegisterCommandHandler(
    const std::string& command,
    const CommandHandler& handler) {
  command_handlers_[command] = handler;
}

void DevToolsProtocol::Handler::SendNotification(
    const std::string& method,
    DictionaryValue* params) {
  DevToolsProtocol::Notification notification(method, params);
  if (!notifier_.is_null())
    notifier_.Run(notification.Serialize());
}

static bool ParseMethod(DictionaryValue* command,
                        std::string* method) {
  if (!command->GetString(kMethodParam, method))
    return false;
  size_t pos = method->find(".");
  if (pos == std::string::npos || pos == 0)
    return false;
  return true;
}

// static
DevToolsProtocol::Command* DevToolsProtocol::ParseCommand(
    const std::string& json,
    std::string* error_response) {
  scoped_ptr<DictionaryValue> command_dict(ParseMessage(json, error_response));
  if (!command_dict)
    return NULL;

  int id;
  std::string method;
  bool ok = command_dict->GetInteger(kIdParam, &id) && id >= 0;
  ok = ok && ParseMethod(command_dict.get(), &method);
  if (!ok) {
    Response response(kNoId, kErrorInvalidRequest, "No such method");
    *error_response = response.Serialize();
    return NULL;
  }

  DictionaryValue* params = NULL;
  command_dict->GetDictionary(kParamsParam, &params);
  return new Command(id, method, params ? params->DeepCopy() : NULL);
}

// static
DevToolsProtocol::Notification*
DevToolsProtocol::ParseNotification(const std::string& json) {
  scoped_ptr<DictionaryValue> dict(ParseMessage(json, NULL));
  if (!dict)
    return NULL;

  std::string method;
  bool ok = ParseMethod(dict.get(), &method);
  if (!ok)
    return NULL;

  DictionaryValue* params = NULL;
  dict->GetDictionary(kParamsParam, &params);
  return new Notification(method, params ? params->DeepCopy() : NULL);
}

//static
DevToolsProtocol::Notification* DevToolsProtocol::CreateNotification(
    const std::string& method,
    DictionaryValue* params) {
  return new Notification(method, params);
}

// static
DictionaryValue* DevToolsProtocol::ParseMessage(
    const std::string& json,
    std::string* error_response) {
  int parse_error_code;
  std::string error_message;
  scoped_ptr<Value> message(
      base::JSONReader::ReadAndReturnError(
          json, 0, &parse_error_code, &error_message));

  if (!message || !message->IsType(Value::TYPE_DICTIONARY)) {
    Response response(0, kErrorParseError, error_message);
    if (error_response)
      *error_response = response.Serialize();
    return NULL;
  }

  return static_cast<DictionaryValue*>(message.release());
}

}  // namespace content
