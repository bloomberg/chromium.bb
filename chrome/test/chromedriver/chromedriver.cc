// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chromedriver.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/status.h"

namespace {

// Guards initialization of |g_command_executor|.
base::LazyInstance<base::Lock> g_lazy_lock = LAZY_INSTANCE_INITIALIZER;

CommandExecutor* g_command_executor = NULL;

CommandExecutor* CreateCommandExecutor() {
  return new CommandExecutorImpl();
}

CommandExecutorFactoryFunc g_executor_factory = &CreateCommandExecutor;

void SetResponse(StatusCode status,
                 const base::Value* value,
                 const std::string& session_id,
                 std::string* response) {
  base::DictionaryValue response_dict;
  response_dict.SetInteger("status", status);
  response_dict.Set("value", value->DeepCopy());
  response_dict.SetString("sessionId", session_id);
  std::string json;
  base::JSONWriter::Write(&response_dict, response);
}

void SetError(const std::string& error_msg,
              std::string* response) {
  base::DictionaryValue value;
  value.SetString("message", error_msg);
  SetResponse(kUnknownError, &value, "", response);
}

}  // namespace

void SetCommandExecutorFactoryForTesting(CommandExecutorFactoryFunc func) {
  g_executor_factory = func;
}

// Synchronously executes the given command. Thread safe.
// Command must be a JSON object:
// {
//   "name": <string>,
//   "parameters": <dictionary>,
//   "sessionId": <string>
// }
// Response will always be a JSON object:
// {
//   "status": <integer>,
//   "value": <object>,
//   "sessionId": <string>
// }
// If "status" is non-zero, "value" will be an object with a string "message"
// property which signifies the error message.
void ExecuteCommand(const std::string& command, std::string* response) {
  std::string error_msg;
  scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      command, 0, NULL, &error_msg));
  if (!value.get()) {
    SetError("failed to parse command: " + error_msg, response);
    return;
  }
  base::DictionaryValue* command_dict;
  if (!value->GetAsDictionary(&command_dict)) {
    SetError("invalid command (must be dictionary)", response);
    return;
  }
  std::string name;
  if (!command_dict->GetString("name", &name)) {
    SetError("invalid command (must contain 'name' of type string)", response);
    return;
  }
  base::DictionaryValue* params;
  if (!command_dict->GetDictionary("parameters", &params)) {
    SetError("invalid command (must contain 'parameters' of type dict)",
             response);
    return;
  }
  std::string session_id;
  if (!command_dict->GetString("sessionId", &session_id)) {
    SetError("invalid command (must contain 'sessionId' of type string)",
             response);
    return;
  }
  StatusCode out_status = kOk;
  scoped_ptr<base::Value> out_value;
  std::string out_session_id;
  {
    base::AutoLock(g_lazy_lock.Get());
    if (!g_command_executor)
      g_command_executor = g_executor_factory();
  }
  g_command_executor->ExecuteCommand(
      name, *params, session_id, &out_status, &out_value, &out_session_id);
  SetResponse(out_status, out_value.get(), out_session_id, response);
}
