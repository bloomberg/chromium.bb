// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/session_with_id.h"

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"

namespace webdriver {

void SessionWithID::ExecuteGet(Response* const response) {
  DictionaryValue *temp_value = new DictionaryValue();

  temp_value->SetString(std::string("browserName"),
                        std::string("chrome"));
  temp_value->SetString(std::string("version"),
                        std::string(chrome::kChromeVersion));

#ifdef OS_MACOSX
  temp_value->SetString(std::string("platform"), std::string("mac"));
#elif OS_WIN32
  temp_value->SetString(std::string("platform"), std::string("windows"));
#elif OS_LINUX && !OS_CHROMEOS
  temp_value->SetString(std::string("platform"), std::string("linux"));
#elif OS_CHROMEOS
  temp_value->SetString(std::string("platform"), std::string("chromeos"));
#else
  temp_value->SetString(std::string("platform"), std::string("unknown"));
#endif

  temp_value->SetBoolean(std::string("javascriptEnabled"), true);

  response->set_status(kSuccess);
  response->set_value(temp_value);
}

void SessionWithID::ExecuteDelete(Response* const response) {
  session_->Terminate();
  SessionManager::GetInstance()->Delete(session_->id());
  response->set_status(kSuccess);
}

}  // namespace webdriver
