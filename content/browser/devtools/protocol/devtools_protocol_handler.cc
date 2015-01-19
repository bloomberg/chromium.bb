// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
const int kStatusParseError = -32700;
const int kStatusInvalidRequest = -32600;
const int kStatusNoSuchMethod = -32601;

scoped_ptr<base::DictionaryValue> TakeDictionary(base::DictionaryValue* dict,
                                                 const std::string& key) {
  scoped_ptr<base::Value> value;
  dict->Remove(key, &value);
  base::DictionaryValue* result = nullptr;
  if (value)
    value.release()->GetAsDictionary(&result);
  return make_scoped_ptr(result);
}

}  // namespace

DevToolsProtocolHandler::DevToolsProtocolHandler(const Notifier& notifier)
    : client_(notifier),
      dispatcher_(notifier) {
}

DevToolsProtocolHandler::~DevToolsProtocolHandler() {
}

scoped_ptr<base::DictionaryValue>
DevToolsProtocolHandler::ParseCommand(const std::string& message) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(message));
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY)) {
    client_.SendError(DevToolsProtocolClient::kNoId,
                      Response(kStatusParseError,
                               "Message must be in JSON format"));
    return nullptr;
  }

  scoped_ptr<base::DictionaryValue> command =
      make_scoped_ptr(static_cast<base::DictionaryValue*>(value.release()));
  int id = DevToolsProtocolClient::kNoId;
  bool ok = command->GetInteger(kIdParam, &id) && id >= 0;
  if (!ok) {
    client_.SendError(id, Response(kStatusInvalidRequest,
                                   "The type of 'id' property must be number"));
    return nullptr;
  }

  std::string method;
  ok = command->GetString(kMethodParam, &method);
  if (!ok) {
    client_.SendError(id,
        Response(kStatusInvalidRequest,
                 "The type of 'method' property must be string"));
    return nullptr;
  }

  return command;
}

void DevToolsProtocolHandler::HandleCommand(
    scoped_ptr<base::DictionaryValue> command) {
  int id = DevToolsProtocolClient::kNoId;
  std::string method;
  command->GetInteger(kIdParam, &id);
  command->GetString(kMethodParam, &method);
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(method));
  if (command_handler.is_null()) {
    client_.SendError(id, Response(kStatusNoSuchMethod, "No such method"));
    return;
  }
  bool result =
      command_handler.Run(id, TakeDictionary(command.get(), kParamsParam));
  DCHECK(result);
}

bool DevToolsProtocolHandler::HandleOptionalCommand(
    scoped_ptr<base::DictionaryValue> command) {
  int id = DevToolsProtocolClient::kNoId;
  std::string method;
  command->GetInteger(kIdParam, &id);
  command->GetString(kMethodParam, &method);
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(method));
  if (!command_handler.is_null())
    return command_handler.Run(id, TakeDictionary(command.get(), kParamsParam));
  return false;
}

}  // namespace content
