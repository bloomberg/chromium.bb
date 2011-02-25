// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webdriver_command.h"

#include <string>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/error_codes.h"

namespace webdriver {

bool WebDriverCommand::Init(Response* const response) {
  // There should be at least 3 path segments to match "/session/$id".
  std::string session_id = GetPathVariable(2);
  if (session_id.length() == 0) {
    SET_WEBDRIVER_ERROR(response, "No session ID specified", kBadRequest);
    return false;
  }

  VLOG(1) << "Fetching session: " << session_id;
  session_ = SessionManager::GetInstance()->GetSession(session_id);
  if (session_ == NULL) {
    SET_WEBDRIVER_ERROR(response, "Session not found: " + session_id,
                        kSessionNotFound);
    return false;
  }

  response->SetField("sessionId", Value::CreateStringValue(session_id));
  return true;
}

}  // namespace webdriver
