// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_protocol_handler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/public/browser/devtools_manager_delegate.h"

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

DevToolsProtocolHandler::DevToolsProtocolHandler(
    DevToolsAgentHost* agent_host, const Notifier& notifier)
    : agent_host_(agent_host),
      client_(notifier),
      dispatcher_(notifier) {
}

DevToolsProtocolHandler::~DevToolsProtocolHandler() {
}

void DevToolsProtocolHandler::HandleMessage(const std::string& message) {
  scoped_ptr<base::DictionaryValue> command = ParseCommand(message);
  if (!command)
    return;
  if (PassCommandToDelegate(command.get()))
    return;
  HandleCommand(command.Pass());
}

bool DevToolsProtocolHandler::HandleOptionalMessage(
    const std::string& message, int* call_id) {
  scoped_ptr<base::DictionaryValue> command = ParseCommand(message);
  if (!command)
    return true;
  if (PassCommandToDelegate(command.get()))
    return true;
  return HandleOptionalCommand(command.Pass(), call_id);
}

bool DevToolsProtocolHandler::PassCommandToDelegate(
    base::DictionaryValue* command) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return false;

  scoped_ptr<base::DictionaryValue> response(
      delegate->HandleCommand(agent_host_, command));
  if (response) {
    std::string json_response;
    base::JSONWriter::Write(*response, &json_response);
    client_.SendRawMessage(json_response);
    return true;
  }

  return false;
}

scoped_ptr<base::DictionaryValue>
DevToolsProtocolHandler::ParseCommand(const std::string& message) {
  scoped_ptr<base::Value> value = base::JSONReader::Read(message);
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
    scoped_ptr<base::DictionaryValue> command, int* call_id) {
  *call_id = DevToolsProtocolClient::kNoId;
  std::string method;
  command->GetInteger(kIdParam, call_id);
  command->GetString(kMethodParam, &method);
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(method));
  if (!command_handler.is_null()) {
    return command_handler.Run(
        *call_id, TakeDictionary(command.get(), kParamsParam));
  }
  return false;
}

}  // namespace content
