// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/default_gcm_app_handler.h"

#include "base/logging.h"

namespace gcm {

DefaultGCMAppHandler::DefaultGCMAppHandler() {
}

DefaultGCMAppHandler::~DefaultGCMAppHandler() {
}

void DefaultGCMAppHandler::ShutdownHandler() {
  // Nothing to do.
}

void DefaultGCMAppHandler::OnMessage(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  LOG(ERROR) << "No app handler is found to route message for " << app_id;
}

void DefaultGCMAppHandler::OnMessagesDeleted(const std::string& app_id) {
  LOG(ERROR) << "No app handler is found to route deleted message for "
             << app_id;
}

void DefaultGCMAppHandler::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  LOG(ERROR) << "No app handler is found to route send error message for "
             << app_id;
}

}  // namespace gcm
