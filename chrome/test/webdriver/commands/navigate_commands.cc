// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/navigate_commands.h"

#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

ForwardCommand::ForwardCommand(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters)
      : WebDriverCommand(path_segments, parameters) {}

ForwardCommand::~ForwardCommand() {}

bool ForwardCommand::DoesPost() {
  return true;
}

void ForwardCommand::ExecutePost(Response* const response) {
  Error* error = session_->GoForward();
  if (error)
    response->SetError(error);
}

BackCommand::BackCommand(const std::vector<std::string>& path_segments,
              const DictionaryValue* const parameters)
      : WebDriverCommand(path_segments, parameters) {}

BackCommand::~BackCommand() {}

bool BackCommand::DoesPost() {
  return true;
}

void BackCommand::ExecutePost(Response* const response) {
  Error* error = session_->GoBack();
  if (error)
    response->SetError(error);
}

RefreshCommand::RefreshCommand(const std::vector<std::string>& path_segments,
                               const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

RefreshCommand::~RefreshCommand() {}

bool RefreshCommand::DoesPost() {
  return true;
}

void RefreshCommand::ExecutePost(Response* const response) {
  Error* error = session_->Reload();
  if (error)
    response->SetError(error);
}

}  // namespace webdriver
