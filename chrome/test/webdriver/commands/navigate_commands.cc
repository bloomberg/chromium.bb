// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/navigate_commands.h"

#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

ForwardCommand::ForwardCommand(const std::vector<std::string>& path_segments,
                 const DictionaryValue* const parameters)
      : WebDriverCommand(path_segments, parameters) {}

ForwardCommand::~ForwardCommand() {}

bool ForwardCommand::DoesPost() {
  return true;
}

void ForwardCommand::ExecutePost(Response* const response) {
  if (!session_->GoForward()) {
    SET_WEBDRIVER_ERROR(response, "GoForward failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->SetStatus(kSuccess);
}

bool ForwardCommand::RequiresValidTab() {
  return true;
}

BackCommand::BackCommand(const std::vector<std::string>& path_segments,
              const DictionaryValue* const parameters)
      : WebDriverCommand(path_segments, parameters) {}

BackCommand::~BackCommand() {}

bool BackCommand::DoesPost() {
  return true;
}

void BackCommand::ExecutePost(Response* const response) {
  if (!session_->GoBack()) {
    SET_WEBDRIVER_ERROR(response, "GoBack failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->SetStatus(kSuccess);
}

bool BackCommand::RequiresValidTab() {
  return true;
}

RefreshCommand::RefreshCommand(const std::vector<std::string>& path_segments,
                               const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

RefreshCommand::~RefreshCommand() {}

bool RefreshCommand::DoesPost() {
  return true;
}

void RefreshCommand::ExecutePost(Response* const response) {
  if (!session_->Reload()) {
    SET_WEBDRIVER_ERROR(response, "Reload failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->SetStatus(kSuccess);
}

bool RefreshCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver
