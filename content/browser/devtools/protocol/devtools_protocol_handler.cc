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

DevToolsProtocolHandler::DevToolsProtocolHandler(bool handle_generic_errors,
                                                 const Notifier& notifier)
    : client_(notifier),
      handle_generic_errors_(handle_generic_errors),
      dispatcher_(notifier) {
}

DevToolsProtocolHandler::~DevToolsProtocolHandler() {
}

scoped_ptr<base::DictionaryValue>
DevToolsProtocolHandler::ParseCommand(const std::string& message) {
  std::string parse_error;
  scoped_ptr<base::Value> value(
      base::JSONReader::ReadAndReturnError(message, 0, nullptr, &parse_error));

  if (!value) {
    if (handle_generic_errors_) {
      client_.SendError(DevToolsProtocolClient::kNoId,
                        Response(kStatusParseError, parse_error));
    }
    return nullptr;
  }
  if (!value->IsType(base::Value::TYPE_DICTIONARY)) {
    if (handle_generic_errors_) {
      client_.SendError(DevToolsProtocolClient::kNoId,
                        Response(kStatusInvalidRequest, "Not a dictionary"));
    }
    return nullptr;
  }
  return make_scoped_ptr(static_cast<base::DictionaryValue*>(value.release()));
}

bool DevToolsProtocolHandler::HandleCommand(
    scoped_ptr<base::DictionaryValue> command) {
  int id = DevToolsProtocolClient::kNoId;
  std::string method;
  bool ok = command->GetInteger(kIdParam, &id) && id >= 0;
  ok = ok && command->GetString(kMethodParam, &method);
  if (!ok) {
    if (handle_generic_errors_) {
      client_.SendError(id, Response(kStatusInvalidRequest, "Invalid request"));
      return true;
    }
    return false;
  }
  DevToolsProtocolDispatcher::CommandHandler command_handler(
      dispatcher_.FindCommandHandler(method));
  if (command_handler.is_null()) {
    if (handle_generic_errors_) {
      client_.SendError(id, Response(kStatusNoSuchMethod, "No such method"));
      return true;
    }
    return false;

  }
  return command_handler.Run(id, TakeDictionary(command.get(), kParamsParam));
}

}  // namespace content
