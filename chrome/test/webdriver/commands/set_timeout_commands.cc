// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/set_timeout_commands.h"

#include <string>

#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

SetTimeoutCommand::SetTimeoutCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters) :
  WebDriverCommand(path_segments, parameters) {}

SetTimeoutCommand::~SetTimeoutCommand() {}

bool SetTimeoutCommand::DoesPost() {
  return true;
}

void SetTimeoutCommand::ExecutePost(Response* const response) {
  // Timeout value in milliseconds
  const char kTimeoutMsKey[] = "ms";

  if (!HasParameter(kTimeoutMsKey)) {
    response->SetError(new Error(kBadRequest, "Request missing ms parameter"));
    return;
  }

  int ms_to_wait;
  if (!GetIntegerParameter(kTimeoutMsKey, &ms_to_wait)) {
    // Client may have sent us a floating point number. Since DictionaryValue
    // will not do a down cast for us, we must explicitly check for it here.
    // Note webdriver only supports whole milliseconds for a timeout value, so
    // we are safe to downcast.
    double ms;
    if (!GetDoubleParameter(kTimeoutMsKey, &ms)) {
      response->SetError(new Error(
          kBadRequest, "ms parameter is not a number"));
      return;
    }
    ms_to_wait = static_cast<int>(ms);
  }

  // Validate the wait time before setting it to the session.
  if (ms_to_wait < 0) {
    response->SetError(new Error(kBadRequest, "Timeout must be non-negative"));
    return;
  }

  SetTimeout(ms_to_wait);
}

SetAsyncScriptTimeoutCommand::SetAsyncScriptTimeoutCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : SetTimeoutCommand(path_segments, parameters) {}

SetAsyncScriptTimeoutCommand::~SetAsyncScriptTimeoutCommand() {}

void SetAsyncScriptTimeoutCommand::SetTimeout(int timeout_ms) {
  session_->set_async_script_timeout(timeout_ms);
}

ImplicitWaitCommand::ImplicitWaitCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : SetTimeoutCommand(path_segments, parameters) {}

ImplicitWaitCommand::~ImplicitWaitCommand() {}

void ImplicitWaitCommand::SetTimeout(int timeout_ms) {
  session_->set_implicit_wait(timeout_ms);
}

}  // namespace webdriver
