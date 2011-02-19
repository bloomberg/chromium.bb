// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/source_command.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

// Private atom to find source code of the page.
const char* const kSource =
    "window.domAutomationController.send("
    "new XMLSerializer().serializeToString(document));";

SourceCommand::SourceCommand(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

SourceCommand::~SourceCommand() {}

bool SourceCommand::DoesGet() {
  return true;
}

void SourceCommand::ExecuteGet(Response* const response) {
  Value* result = NULL;

  scoped_ptr<ListValue> list(new ListValue());
  if (!session_->ExecuteScript(kSource, list.get(), &result)) {
    LOG(ERROR) << "Could not execute JavaScript to find source. JavaScript"
               << " used was:\n" << kSource;
    LOG(ERROR) << "ExecuteAndExtractString's results was: "
               << result;
    SET_WEBDRIVER_ERROR(response, "ExecuteAndExtractString failed",
                        kInternalServerError);
    return;
  }
  response->SetValue(result);
  response->SetStatus(kSuccess);
}

bool SourceCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver
