// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/appcache_status_command.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

AppCacheStatusCommand::AppCacheStatusCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

AppCacheStatusCommand::~AppCacheStatusCommand() {}

bool AppCacheStatusCommand::DoesGet() {
  return true;
}

void AppCacheStatusCommand::ExecuteGet(Response* const response) {
  int status;
  Error* error = session_->GetAppCacheStatus(&status);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(status));
}

}  // namespace webdriver
