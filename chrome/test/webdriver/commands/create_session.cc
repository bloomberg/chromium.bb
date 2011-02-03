// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/create_session.h"

#include <sstream>
#include <string>

#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"

namespace webdriver {

void CreateSession::ExecutePost(Response* const response) {
  SessionManager* session_manager = SessionManager::GetInstance();
  Session* session = session_manager->Create();
  if (!session) {
    SET_WEBDRIVER_ERROR(response,
                        "Failed to create session",
                        kInternalServerError);
    return;
  }

  std::string session_id = session->id();
  if (!session->Init()) {
    session_manager->Delete(session_id);
    SET_WEBDRIVER_ERROR(response,
                        "Failed to initialize session",
                        kInternalServerError);
    return;
  }

  VLOG(1) << "Created session " << session_id;
  std::ostringstream stream;
  stream << "http://" << session_manager->GetIPAddress() << "/session/"
         << session_id;
  response->set_status(kSeeOther);
  response->set_value(Value::CreateStringValue(stream.str()));
}

}  // namespace webdriver
