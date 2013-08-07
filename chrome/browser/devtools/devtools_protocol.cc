// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_protocol.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

namespace {
const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";
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

DevToolsProtocol::Notification::~Notification() {
}

DevToolsProtocol::Notification::Notification(const std::string& method,
                                             base::DictionaryValue* params)
    : Message(method, params) {
}

// static
DevToolsProtocol::Notification* DevToolsProtocol::ParseNotification(
    const std::string& json) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  if (!value || !value->IsType(Value::TYPE_DICTIONARY))
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
