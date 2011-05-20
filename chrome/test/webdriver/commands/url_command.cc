// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/url_command.h"

#include <string>

#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/webdriver_error.h"

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
  Error* error = session_->GetURL(&url);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new StringValue(url));
}

void URLCommand::ExecutePost(Response* const response) {
  std::string url;

  if (!GetStringASCIIParameter("url", &url)) {
    response->SetError(new Error(kBadRequest, "Missing 'url' parameter"));
    return;
  }

  Error* error = session_->NavigateToURL(url);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new StringValue(url));
}


}  // namespace webdriver
