// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/session_with_id.h"

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_session.h"

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

  // Standard capabilities defined at
  // http://code.google.com/p/selenium/wiki/JsonWireProtocol#Capabilities_JSON_Object
  temp_value->SetString("browserName", "chrome");
  temp_value->SetString("version", session_->GetBrowserVersion());

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
  temp_value->SetBoolean("takesScreenshot", true);
  temp_value->SetBoolean("handlesAlerts", true);
  temp_value->SetBoolean("databaseEnabled", false);
  temp_value->SetBoolean("locationContextEnabled", false);
  temp_value->SetBoolean("applicationCacheEnabled", false);
  temp_value->SetBoolean("browserConnectionEnabled", false);
  temp_value->SetBoolean("cssSelectorsEnabled", true);
  temp_value->SetBoolean("webStorageEnabled", true);
  temp_value->SetBoolean("rotatable", false);
  temp_value->SetBoolean("acceptSslCerts", false);
  // Even when ChromeDriver does not OS-events, the input simulation produces
  // the same effect for most purposes (except IME).
  temp_value->SetBoolean("nativeEvents", true);

  // Custom non-standard session info.
  temp_value->SetWithoutPathExpansion(
      "chrome.chromedriverVersion",
      Value::CreateStringValue(chrome::kChromeVersion));

  response->SetValue(temp_value);
}

void SessionWithID::ExecuteDelete(Response* const response) {
  // Session manages its own lifetime, so do not call delete.
  session_->Terminate();
}

bool SessionWithID::ShouldRunPreAndPostCommandHandlers() {
  return false;
}

}  // namespace webdriver
