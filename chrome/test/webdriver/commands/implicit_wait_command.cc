// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/test/webdriver/commands/implicit_wait_command.h"

namespace webdriver {

ImplicitWaitCommand::ImplicitWaitCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters),
      ms_to_wait_(0) {}

ImplicitWaitCommand::~ImplicitWaitCommand() {}

bool ImplicitWaitCommand::Init(Response* const response) {
  if (!(WebDriverCommand::Init(response))) {
    SET_WEBDRIVER_ERROR(response, "Failure on Init for find element",
                        kInternalServerError);
    return false;
  }

  // Record the requested wait time.
  if (!GetIntegerParameter("ms", &ms_to_wait_)) {
    SET_WEBDRIVER_ERROR(response, "Request missing ms parameter",
                        kBadRequest);
    return false;
  }

  return true;
}

bool ImplicitWaitCommand::DoesPost() {
  return true;
}

void ImplicitWaitCommand::ExecutePost(Response* const response) {
  // Validate the wait time before setting it to the session.
  if (ms_to_wait_ < 0) {
    SET_WEBDRIVER_ERROR(response, "Wait must be non-negative",
                        kBadRequest);
    return;
  }

  session_->set_implicit_wait(ms_to_wait_);
  LOG(INFO) << "Implicit wait set to: " << ms_to_wait_ << " ms";

  response->set_value(new StringValue("success"));
  response->set_status(kSuccess);
}

bool ImplicitWaitCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver

