// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/session_with_id.h"

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"

namespace webdriver {

SessionWithID::SessionWithID(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

SessionWithID::~SessionWithID() {}

bool SessionWithID::DoesGet() {
  return true;
}

bool SessionWithID::DoesDelete() {
  return true;
}

void SessionWithID::ExecuteGet(Response* const response) {
  DictionaryValue *temp_value = new DictionaryValue();

  temp_value->SetString("browserName", "chrome");
  temp_value->SetString("version", session_->GetVersion());

#if defined(OS_WIN)
  temp_value->SetString("platform", "windows");
#elif defined(OS_MACOSX)
  temp_value->SetString("platform", "mac");
#elif defined(OS_CHROMEOS)
  temp_value->SetString("platform", "chromeos");
#elif defined(OS_LINUX)
  temp_value->SetString("platform", "linux");
#else
  temp_value->SetString("platform", "unknown");
#endif

  temp_value->SetBoolean("javascriptEnabled", true);

  // Custom non-standard session info.
  temp_value->SetString("chrome.chromedriverVersion", "1.0");
  temp_value->SetString("chrome.automationVersion", chrome::kChromeVersion);

  response->SetStatus(kSuccess);
  response->SetValue(temp_value);
}

void SessionWithID::ExecuteDelete(Response* const response) {
  // Session manages its own lifetime, so do not call delete.
  session_->Terminate();
  response->SetStatus(kSuccess);
}

bool SessionWithID::RequiresValidTab() {
  return false;
}

}  // namespace webdriver
