// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_protocol.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"

namespace {

const char kErrorCodeParam[] = "code";
const char kErrorParam[] = "error";
const char kErrorMessageParam[] = "message";
const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";
const char kResultParam[] = "result";

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
enum Error {
  kErrorInvalidParams = -32602
};

}  // namespace

DevToolsProtocol::Message::~Message() {
}

DevToolsProtocol::Message::Message(const std::string& method,
                                   base::DictionaryValue* params)
    : method_(method),
      params_(params ? params->DeepCopy() : NULL) {
}

DevToolsProtocol::Command::Command(int id,
                                   const std::string& method,
                                   base::DictionaryValue* params)
    : Message(method, params),
      id_(id) {
}

DevToolsProtocol::Command::~Command() {
}

std::string DevToolsProtocol::Command::Serialize() {
  base::DictionaryValue command;
  command.SetInteger(kIdParam, id_);
  command.SetString(kMethodParam, method_);
  if (params_)
    command.Set(kParamsParam, params_->DeepCopy());

  std::string json_command;
  base::JSONWriter::Write(&command, &json_command);
  return json_command;
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::SuccessResponse(base::DictionaryValue* result) {
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, result));
}

scoped_ptr<DevToolsProtocol::Response>
DevToolsProtocol::Command::InvalidParamResponse(const std::string& param) {
  std::string message =
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str());
  return scoped_ptr<DevToolsProtocol::Response>(
      new DevToolsProtocol::Response(id_, kErrorInvalidParams, message));
}

DevToolsProtocol::Notification::~Notification() {
}

DevToolsProtocol::Notification::Notification(const std::string& method,
                                             base::DictionaryValue* params)
    : Message(method, params) {
}

DevToolsProtocol::Response::~Response() {
}

DevToolsProtocol::Response::Response(int id,
                                     int error_code,
                                     const std::string error_message)
    : id_(id),
      error_code_(error_code),
      error_message_(error_message) {
}

DevToolsProtocol::Response::Response(int id, base::DictionaryValue* result)
    : id_(id),
      error_code_(0),
      result_(result) {
}

base::DictionaryValue* DevToolsProtocol::Response::Serialize() {
  base::DictionaryValue* response = new base::DictionaryValue();

  response->SetInteger(kIdParam, id_);

  if (error_code_) {
    base::DictionaryValue* error_object = new base::DictionaryValue();
    response->Set(kErrorParam, error_object);
    error_object->SetInteger(kErrorCodeParam, error_code_);
    if (!error_message_.empty())
      error_object->SetString(kErrorMessageParam, error_message_);
  } else {
    if (result_)
      response->Set(kResultParam, result_->DeepCopy());
    else
      response->Set(kResultParam, new base::DictionaryValue());
  }

  return response;
}

// static
DevToolsProtocol::Command* DevToolsProtocol::ParseCommand(
    base::DictionaryValue* command_dict) {
  if (!command_dict)
    return NULL;

  int id;
  if (!command_dict->GetInteger(kIdParam, &id) || id < 0)
    return NULL;

  std::string method;
  if (!command_dict->GetString(kMethodParam, &method))
    return NULL;

  base::DictionaryValue* params = NULL;
  command_dict->GetDictionary(kParamsParam, &params);
  return new Command(id, method, params);
}

// static
DevToolsProtocol::Notification* DevToolsProtocol::ParseNotification(
    const std::string& json) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;

  scoped_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  std::string method;
  if (!dict->GetString(kMethodParam, &method))
    return NULL;

  base::DictionaryValue* params = NULL;
  dict->GetDictionary(kParamsParam, &params);
  return new Notification(method, params);
}

DevToolsProtocol::Response* DevToolsProtocol::ParseResponse(
    const std::string& json) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;

  scoped_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  int id;
  if (!dict->GetInteger(kIdParam, &id))
    return NULL;

  int error_code = 0;
  base::DictionaryValue* error_dict = NULL;
  if (dict->GetDictionary(kErrorParam, &error_dict))
    error_dict->GetInteger(kErrorCodeParam, &error_code);
  return new Response(id, error_code, std::string());
}
