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

CreateSession::CreateSession(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : Command(path_segments, parameters) {}

CreateSession::~CreateSession() {}

bool CreateSession::DoesPost() { return true; }

void CreateSession::ExecutePost(Response* const response) {
  // Session manages its own liftime, so do not call delete.
  Session* session = new Session();
  if (!session->Init()) {
    SET_WEBDRIVER_ERROR(response,
                        "Failed to initialize session",
                        kInternalServerError);
    return;
  }

  SessionManager* session_manager = SessionManager::GetInstance();
  VLOG(1) << "Created session " << session->id();
  std::ostringstream stream;
  stream << "http://" << session_manager->GetAddress() << "/session/"
         << session->id();
  response->set_status(kSeeOther);
  response->set_value(Value::CreateStringValue(stream.str()));
}

}  // namespace webdriver
