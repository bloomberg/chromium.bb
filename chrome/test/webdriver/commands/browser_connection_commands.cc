// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/browser_connection_commands.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

BrowserConnectionCommand::BrowserConnectionCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

BrowserConnectionCommand::~BrowserConnectionCommand() {}

bool BrowserConnectionCommand::DoesGet() {
  return true;
}

void BrowserConnectionCommand::ExecuteGet(Response* const response) {
  bool online;
  Error* error = session_->GetBrowserConnectionState(&online);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(online));
}

}  // namespace webdriver
