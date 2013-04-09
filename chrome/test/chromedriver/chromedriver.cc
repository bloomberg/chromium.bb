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
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_executor.h"

namespace {

// Guards |g_executor_initialized|.
base::LazyInstance<base::Lock> g_lazy_lock = LAZY_INSTANCE_INITIALIZER;
bool g_executor_initialized = false;
CommandExecutor* g_command_executor = NULL;

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
  SetResponse(kUnknownError, &value, std::string(), response);
}

}  // namespace

void Init(scoped_ptr<CommandExecutor> executor) {
  g_command_executor = executor.release();
  // We do not call CommandExecutor::Init here because you can't do some things
  // (e.g., creating threads) during DLL loading on Windows.
}

void ExecuteCommand(const std::string& command, std::string* response) {
  CHECK(g_command_executor);
  {
    base::AutoLock(g_lazy_lock.Get());
    if (!g_executor_initialized) {
      g_command_executor->Init();
      g_executor_initialized = true;
    }
  }
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
  g_command_executor->ExecuteCommand(
      name, *params, session_id, &out_status, &out_value, &out_session_id);
  SetResponse(out_status, out_value.get(), out_session_id, response);
}

void Shutdown() {
  // TODO: Move this out to a separate doc.
  // On shutdown, it is nice to quit all running sessions so that we don't
  // have leftover Chrome processes and temporary user data dirs.
  // To do this, we execute the quitAll command.
  // Alternative shutdown behaviors:
  // 1. If the user doesn't quit the session, don't clean it up.
  //    This is what the FF driver does, except the temp dir is
  //    cleaned up in FF because the client side is responsible for creating
  //    the temp directory, not the driver.
  // 2. Separate helper process that we spawn that is in charge of
  //    launching processes and creating temporary files. This process
  //    communicates to the helper via a socket. The helper process
  //    kills all processes and deletes temp files if it sees the parent
  //    die and then exits itself. This is more complicated, but it guarantees
  //    that the processes and temp dirs are cleaned up, even if this process
  //    exits abnormally.
  // 3. Add Chrome command-line switch for socket/pipe that Chrome listens
  //    to and exits if the pipe is closed. This is how the old
  //    TestingAutomationProvider worked. However, this doesn't clean up
  //    temp directories, unless we make Chrome clean its own directory too.
  //    If Chrome crashes the directory would be leaked.
  base::DictionaryValue params;
  StatusCode status_code;
  scoped_ptr<base::Value> value;
  std::string session_id;
  g_command_executor->ExecuteCommand(
      "quitAll", params, std::string(), &status_code, &value, &session_id);
  delete g_command_executor;
}
