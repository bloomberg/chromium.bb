// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/execute_async_script_command.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"

namespace webdriver {

ExecuteAsyncScriptCommand::ExecuteAsyncScriptCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ExecuteAsyncScriptCommand::~ExecuteAsyncScriptCommand() {}

bool ExecuteAsyncScriptCommand::DoesPost() {
  return true;
}

void ExecuteAsyncScriptCommand::ExecutePost(Response* const response) {
  std::string script;
  if (!GetStringParameter("script", &script)) {
    SET_WEBDRIVER_ERROR(response, "No script to execute specified",
                        kBadRequest);
    return;
  }

  ListValue* args;
  if (!GetListParameter("args", &args)) {
    SET_WEBDRIVER_ERROR(response, "No script arguments specified",
                        kBadRequest);
    return;
  }

  Value* result = NULL;
  ErrorCode status = session_->ExecuteAsyncScript(
      session_->current_target(), script, args, &result);
  response->SetStatus(status);
  response->SetValue(result);
}

}  // namspace webdriver

