// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/title_command.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

TitleCommand::TitleCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

TitleCommand::~TitleCommand() {}

bool TitleCommand::DoesGet() {
  return true;
}

void TitleCommand::ExecuteGet(Response* const response) {
  std::string title;
  ErrorCode code = session_->GetTitle(&title);
  if (code == kSuccess)
    response->SetValue(new StringValue(title));
  response->SetStatus(code);
}

bool TitleCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver
