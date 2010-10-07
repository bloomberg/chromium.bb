// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/test/webdriver/commands/title_command.h"

namespace webdriver {

void TitleCommand::ExecuteGet(Response* const response) {
  std::wstring title;
  if (!tab_->GetTabTitle(&title)) {
    SET_WEBDRIVER_ERROR(response, "GetTabTitle failed", kInternalServerError);
    return;
  }

  response->set_value(new StringValue(WideToUTF16(title)));
  response->set_status(kSuccess);
}

}  // namespace webdriver

