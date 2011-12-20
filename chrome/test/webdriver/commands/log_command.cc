// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/log_command.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

LogCommand::LogCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

LogCommand::~LogCommand() {
}

bool LogCommand::DoesPost() {
  return true;
}

void LogCommand::ExecutePost(Response* const response) {
  std::string type;
  if (!GetStringParameter("type", &type)) {
    response->SetError(new Error(kUnknownError, "Missing or invalid 'type'"));
    return;
  }

  LogType log_type;
  if (!LogType::FromString(type, &log_type)) {
    response->SetError(new Error(kUnknownError, "Unknown log type: " + type));
    return;
  }

  if (log_type.type() == LogType::kDriver) {
    response->SetValue(session_->GetLog());
  } else {
    response->SetError(new Error(kUnknownError, "Unrecognized type: " + type));
    return;
  }
}

}  // namespace webdriver
