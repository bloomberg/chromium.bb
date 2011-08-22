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
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"

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

  session_ = SessionManager::GetInstance()->GetSession(session_id);
  if (session_ == NULL) {
    response->SetError(
        new Error(kSessionNotFound, "Session not found: " + session_id));
    return false;
  }

  Error* error = session_->BeforeExecuteCommand();
  if (error) {
    response->SetError(error);
    return false;
  }
  response->SetField("sessionId", Value::CreateStringValue(session_id));
  return true;
}

void WebDriverCommand::Finish() {
  scoped_ptr<Error> error(session_->AfterExecuteCommand());
  if (error.get()) {
    LOG(WARNING) << "Command did not finish successfully: "
                 << error->GetErrorMessage();
  }
}

}  // namespace webdriver
