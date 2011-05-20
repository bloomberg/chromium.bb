// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webdriver_command.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

WebDriverCommand::WebDriverCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : Command(path_segments, parameters), session_(NULL) {
}

WebDriverCommand::~WebDriverCommand() {}

bool WebDriverCommand::Init(Response* const response) {
  // There should be at least 3 path segments to match "/session/$id".
  std::string session_id = GetPathVariable(2);
  if (session_id.length() == 0) {
    response->SetError(
        new Error(kBadRequest, "No session ID specified"));
    return false;
  }

  VLOG(1) << "Fetching session: " << session_id;
  session_ = SessionManager::GetInstance()->GetSession(session_id);
  if (session_ == NULL) {
    response->SetError(
        new Error(kSessionNotFound, "Session not found: " + session_id));
    return false;
  }

  // TODO(kkania): Do not use the standard automation timeout for this,
  // and throw an error if it does not succeed.
  scoped_ptr<Error> error(session_->WaitForAllTabsToStopLoading());
  if (error.get()) {
    LOG(WARNING) << error->ToString();
  }
  error.reset(session_->SwitchToTopFrameIfCurrentFrameInvalid());
  if (error.get()) {
    LOG(WARNING) << error->ToString();
  }

  response->SetField("sessionId", Value::CreateStringValue(session_id));
  return true;
}

}  // namespace webdriver
