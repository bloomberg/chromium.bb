// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/navigate_commands.h"

namespace webdriver {

void ForwardCommand::ExecutePost(Response* const response) {
  if (!session_->GoForward()) {
    SET_WEBDRIVER_ERROR(response, "GoForward failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->set_status(kSuccess);
}

void BackCommand::ExecutePost(Response* const response) {
  if (!session_->GoBack()) {
    SET_WEBDRIVER_ERROR(response, "GoBack failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->set_status(kSuccess);
}

void RefreshCommand::ExecutePost(Response* const response) {
  if (!session_->Reload()) {
    SET_WEBDRIVER_ERROR(response, "Reload failed", kInternalServerError);
    return;
  }

  session_->set_current_frame_xpath("");
  response->set_status(kSuccess);
}

}  // namespace webdriver
