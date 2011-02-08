// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/webdriver/commands/url_command.h"

namespace webdriver {

void URLCommand::ExecuteGet(Response* const response) {
  std::string url;
  if (!session_->GetURL(&url)) {
    SET_WEBDRIVER_ERROR(response, "GetCurrentURL failed", kInternalServerError);
    return;
  }

  response->set_value(new StringValue(url));
  response->set_status(kSuccess);
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

  session_->set_current_frame_xpath("");
  response->set_value(new StringValue(url));
  response->set_status(kSuccess);
}

}  // namespace webdriver
