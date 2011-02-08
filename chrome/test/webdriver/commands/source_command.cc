// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/source_command.h"

namespace webdriver {

// Private atom to find source code of the page.
const wchar_t* const kSource[] = {
    L"window.domAutomationController.send(",
    L"new XMLSerializer().serializeToString(document));",
};

void SourceCommand::ExecuteGet(Response* const response) {
  std::string jscript = build_atom(kSource, sizeof kSource);
  Value* result = NULL;

  scoped_ptr<ListValue> list(new ListValue());
  if (!session_->ExecuteScript(jscript, list.get(), &result)) {
    LOG(ERROR) << "Could not execute JavaScript to find source. JavaScript"
               << " used was:\n" << kSource;
    LOG(ERROR) << "ExecuteAndExtractString's results was: "
               << result;
    SET_WEBDRIVER_ERROR(response, "ExecuteAndExtractString failed",
                        kInternalServerError);
    return;
  }
  response->set_value(result);
  response->set_status(kSuccess);
}

}  // namespace webdriver
