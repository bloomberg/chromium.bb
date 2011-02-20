// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/implicit_wait_command.h"

#include <string>

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

ImplicitWaitCommand::ImplicitWaitCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ImplicitWaitCommand::~ImplicitWaitCommand() {}

bool ImplicitWaitCommand::DoesPost() {
  return true;
}

void ImplicitWaitCommand::ExecutePost(Response* const response) {
  if (!HasParameter("ms")) {
    SET_WEBDRIVER_ERROR(response, "Request missing ms parameter", kBadRequest);
    return;
  }

  int ms_to_wait;
  if (!GetIntegerParameter("ms", &ms_to_wait)) {
    // Client may have sent us a floating point number. Since DictionaryValue
    // will not do a down cast for us, we must explicitly check for it here.
    // Note webdriver only supports whole milliseconds for a timeout value, so
    // we are safe to downcast.
    double ms;
    if (!GetDoubleParameter("ms", &ms)) {
      SET_WEBDRIVER_ERROR(response, "ms parameter is not a number",
                          kBadRequest);
      return;
    }
    ms_to_wait = static_cast<int>(ms);
  }

  // Validate the wait time before setting it to the session.
  if (ms_to_wait < 0) {
    SET_WEBDRIVER_ERROR(response,
        base::StringPrintf("Wait must be non-negative: %d", ms_to_wait).c_str(),
        kBadRequest);
    return;
  }

  session_->set_implicit_wait(ms_to_wait);
  LOG(INFO) << "Implicit wait set to: " << ms_to_wait << " ms";

  response->SetValue(new StringValue("success"));
  response->SetStatus(kSuccess);
}

bool ImplicitWaitCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver

