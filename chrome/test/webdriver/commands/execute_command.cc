// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/execute_command.h"

#include <string>

#include "base/values.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

const char kArgs[] = "args";
const char kScript[] = "script";

ExecuteCommand::ExecuteCommand(const std::vector<std::string>& path_segments,
                               const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ExecuteCommand::~ExecuteCommand() {}

bool ExecuteCommand::DoesPost() {
  return true;
}

void ExecuteCommand::ExecutePost(Response* const response) {
  std::string script;
  if (!GetStringParameter(kScript, &script)) {
    SET_WEBDRIVER_ERROR(response, "No script to execute specified",
                        kBadRequest);
    return;
  }

  ListValue* args;
  if (!GetListParameter(kArgs, &args)) {
    SET_WEBDRIVER_ERROR(response, "No script arguments specified",
                        kBadRequest);
    return;
  }

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args, &result);
  response->SetStatus(status);
  response->SetValue(result);
}

bool ExecuteCommand::RequiresValidTab() {
  return true;
}

}  // namspace webdriver

