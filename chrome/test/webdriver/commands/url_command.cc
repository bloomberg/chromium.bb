// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/url_command.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"

namespace webdriver {

URLCommand::URLCommand(const std::vector<std::string>& path_segments,
                       const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

URLCommand::~URLCommand() {}

bool URLCommand::DoesGet() {
  return true;
}

bool URLCommand::DoesPost() {
  return true;
}

void URLCommand::ExecuteGet(Response* const response) {
  std::string url;
  if (!session_->GetURL(&url)) {
    SET_WEBDRIVER_ERROR(response, "GetCurrentURL failed", kInternalServerError);
    return;
  }

  response->SetValue(new StringValue(url));
  response->SetStatus(kSuccess);
}

void URLCommand::ExecutePost(Response* const response) {
  std::string url;

  if (!GetStringASCIIParameter("url", &url)) {
    SET_WEBDRIVER_ERROR(response, "URL field not found", kInternalServerError);
    return;
  }
  // TODO(jmikhail): sniff for meta-redirects.
  if (!session_->NavigateToURL(url)) {
    SET_WEBDRIVER_ERROR(response, "NavigateToURL failed",
                        kInternalServerError);
    return;
  }

  response->SetValue(new StringValue(url));
  response->SetStatus(kSuccess);
}

bool URLCommand::RequiresValidTab() {
  return true;
}

}  // namespace webdriver
