// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/default_gcm_app_handler.h"

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
  DVLOG(1) << "No app handler is found to route message for " << app_id;
}

void DefaultGCMAppHandler::OnMessagesDeleted(const std::string& app_id) {
  DVLOG(1) << "No app handler is found to route deleted message for " << app_id;
}

void DefaultGCMAppHandler::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DVLOG(1) << "No app handler is found to route send error message for "
           << app_id;
}

void DefaultGCMAppHandler::OnSendAcknowledged(const std::string& app_id,
                                              const std::string& message_id) {
  DVLOG(1) << "No app handler is found to route send acknoledgement for "
           << app_id;
}

}  // namespace gcm
