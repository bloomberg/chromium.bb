// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

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

}  // namespace

using base::DictionaryValue;
using base::Value;

DevToolsProtocol::Command::~Command() {
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::SuccessResponse(base::DictionaryValue* result) {
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, result));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::ErrorResponse(int error_code,
                                         const std::string& error_message) {
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, error_code, error_message));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::NoSuchMethodErrorResponse() {
  return scoped_ptr<DevToolsProtocol::Response>(
      new Response(id_, kErrorNoSuchMethod, "No such method"));
}

DevToolsProtocol::Command::Command(int id,
                                   const std::string& domain,
                                   const std::string& method,
                                   DictionaryValue* params)
    : id_(id),
      domain_(domain),
      method_(method),
      params_(params) {
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
  } else if (result_.get()) {
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
    : method_(method),
      params_(params) {
}

DevToolsProtocol::Notification::~Notification() {
}

std::string DevToolsProtocol::Notification::Serialize() {
  DictionaryValue response;

  base::DictionaryValue notification;
  notification.SetString("method", method_);
  if (params_.get())
    notification.Set("params", params_->DeepCopy());

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
    base::DictionaryValue* params) {
  DevToolsProtocol::Notification notification(method, params);
  if (!notifier_.is_null())
    notifier_.Run(notification.Serialize());
}

// static
DevToolsProtocol::Command* DevToolsProtocol::ParseCommand(
    const std::string& json,
    std::string* error_response) {
  int parse_error_code;
  std::string error_message;
  scoped_ptr<base::Value> command(
      base::JSONReader::ReadAndReturnError(
          json, 0, &parse_error_code, &error_message));

  if (!command || !command->IsType(base::Value::TYPE_DICTIONARY)) {
    Response response(0, kErrorParseError, error_message);
    *error_response = response.Serialize();
    return NULL;
  }

  base::DictionaryValue* command_dict = NULL;
  command->GetAsDictionary(&command_dict);

  int id;
  std::string method;
  bool ok = true;
  ok &= command_dict->GetInteger("id", &id);
  ok &= id >= 0;
  ok &= command_dict->GetString("method", &method);
  if (!ok) {
    Response response(kNoId, kErrorInvalidRequest, "Invalid request");
    *error_response = response.Serialize();
    return NULL;
  }

  size_t pos = method.find(".");
  if (pos == std::string::npos || pos == 0) {
    Response response(kNoId, kErrorNoSuchMethod, "No such method");
    *error_response = response.Serialize();
    return NULL;
  }

  std::string domain = method.substr(0, pos);

  base::DictionaryValue* params = NULL;
  command_dict->GetDictionary("params", &params);
  return new Command(id, domain, method, params ? params->DeepCopy() : NULL);
}

}  // namespace content
