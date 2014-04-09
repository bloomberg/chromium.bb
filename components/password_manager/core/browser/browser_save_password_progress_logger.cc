// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

BrowserSavePasswordProgressLogger::BrowserSavePasswordProgressLogger(
    PasswordManagerClient* client)
    : client_(client) {
  DCHECK(client_);
}

BrowserSavePasswordProgressLogger::~BrowserSavePasswordProgressLogger() {}

void BrowserSavePasswordProgressLogger::SendLog(const std::string& log) {
  client_->LogSavePasswordProgress(log);
}

}  // namespace password_manager
