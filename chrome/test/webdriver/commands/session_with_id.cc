// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/session_with_id.h"

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
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

  temp_value->SetString(std::string("browserName"),
                        std::string("chrome"));
  temp_value->SetString(std::string("version"),
                        std::string(chrome::kChromeVersion));

#if defined(OS_WIN)
  temp_value->SetString(std::string("platform"), std::string("windows"));
#elif defined(OS_MACOSX)
  temp_value->SetString(std::string("platform"), std::string("mac"));
#elif defined(OS_CHROMEOS)
  temp_value->SetString(std::string("platform"), std::string("chromeos"));
#elif defined(OS_LINUX)
  temp_value->SetString(std::string("platform"), std::string("linux"));
#else
  temp_value->SetString(std::string("platform"), std::string("unknown"));
#endif

  temp_value->SetBoolean(std::string("javascriptEnabled"), true);

  response->set_status(kSuccess);
  response->set_value(temp_value);
}

void SessionWithID::ExecuteDelete(Response* const response) {
  // Session manages its own liftime, so do not call delete.
  session_->Terminate();
  response->set_status(kSuccess);
}

bool SessionWithID::RequiresValidTab() {
  return false;
}

}  // namespace webdriver
