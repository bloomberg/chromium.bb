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

// static
std::string DevToolsProtocol::SerializeCommand(
    int command_id,
    const std::string& method,
    std::unique_ptr<base::DictionaryValue> params) {
  base::DictionaryValue command;
  command.SetInteger(kIdParam, command_id);
  command.SetString(kMethodParam, method);
  if (params)
    command.Set(kParamsParam, params.release());

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  return json_command;
}

// static
std::unique_ptr<base::DictionaryValue>
DevToolsProtocol::CreateInvalidParamsResponse(int command_id,
                                              const std::string& param) {
  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  base::DictionaryValue* error_object = new base::DictionaryValue();
  response->Set(kErrorParam, error_object);
  error_object->SetInteger(kErrorCodeParam, kErrorInvalidParams);
  error_object->SetString(kErrorMessageParam,
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str()));

  return response;
}

// static
std::unique_ptr<base::DictionaryValue> DevToolsProtocol::CreateSuccessResponse(
    int command_id,
    std::unique_ptr<base::DictionaryValue> result) {
  std::unique_ptr<base::DictionaryValue> response(new base::DictionaryValue());
  response->SetInteger(kIdParam, command_id);
  response->Set(kResultParam,
                result ? result.release() : new base::DictionaryValue());

  return response;
}

// static
bool DevToolsProtocol::ParseCommand(base::DictionaryValue* command,
                                    int* command_id,
                                    std::string* method,
                                    base::DictionaryValue** params) {
  if (!command)
    return false;

  if (!command->GetInteger(kIdParam, command_id) || *command_id < 0)
    return false;

  if (!command->GetString(kMethodParam, method))
    return false;

  if (!command->GetDictionary(kParamsParam, params))
    *params = nullptr;

  return true;
}

// static
bool DevToolsProtocol::ParseNotification(
    const std::string& json,
    std::string* method,
    std::unique_ptr<base::DictionaryValue>* params) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->IsType(base::Value::Type::DICTIONARY))
    return false;

  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  if (!dict->GetString(kMethodParam, method))
    return false;

  std::unique_ptr<base::Value> params_value;
  dict->Remove(kParamsParam, &params_value);
  if (params_value && params_value->IsType(base::Value::Type::DICTIONARY))
    params->reset(static_cast<base::DictionaryValue*>(params_value.release()));

  return true;
}

// static
bool DevToolsProtocol::ParseResponse(const std::string& json,
                                     int* command_id,
                                     int* error_code) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->IsType(base::Value::Type::DICTIONARY))
    return false;

  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  if (!dict->GetInteger(kIdParam, command_id))
    return false;

  *error_code = 0;
  base::DictionaryValue* error_dict = nullptr;
  if (dict->GetDictionary(kErrorParam, &error_dict))
    error_dict->GetInteger(kErrorCodeParam, error_code);
  return true;
}
