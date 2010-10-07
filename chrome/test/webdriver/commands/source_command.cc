// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/source_command.h"

namespace webdriver {

// Private atom to find source code of the page.
const wchar_t* const kSource[] = {
    L"window.domAutomationController.send(",
    L"new XMLSerializer().serializeToString(document));",
};

void SourceCommand::ExecuteGet(Response* const response) {
  const std::wstring jscript = build_atom(kSource, sizeof kSource);
  // Get the source code for the current frame only.
  std::wstring xpath = session_->current_frame_xpath();
  std::wstring result = L"";

  if (!tab_->ExecuteAndExtractString(xpath, jscript, &result)) {
    LOG(ERROR) << "Could not execute JavaScript to find source. JavaScript"
               << " used was:\n" << kSource;
    LOG(ERROR) << "ExecuteAndExtractString's results was: "
               << result;
    SET_WEBDRIVER_ERROR(response, "ExecuteAndExtractString failed",
                        kInternalServerError);
    return;
  }

  response->set_value(new StringValue(WideToUTF16(result)));
  response->set_status(kSuccess);
}

}  // namespace webdriver

