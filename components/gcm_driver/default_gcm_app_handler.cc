// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/default_gcm_app_handler.h"

#include "base/logging.h"
#include "net/base/ip_endpoint.h"

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

void DefaultGCMAppHandler::OnConnected(const net::IPEndPoint& ip_endpoint) {
  // TODO(semenzato): update CrOS NIC state.
  DVLOG(1) << "GCM connected to " << ip_endpoint.ToString();
}

void DefaultGCMAppHandler::OnDisconnected() {
  // TODO(semenzato): update CrOS NIC state.
  DVLOG(1) << "GCM disconnected";
}

}  // namespace gcm
