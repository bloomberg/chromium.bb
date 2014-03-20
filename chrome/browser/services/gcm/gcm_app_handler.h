// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_APP_HANDLER_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_APP_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

// Defines the interface to provide handling and event routing logic for a given
// app.
class GCMAppHandler {
 public:
  GCMAppHandler() {}
  virtual ~GCMAppHandler() {}

  // Called to do all the cleanup when GCM is shutting down.
  // In the case that multiple apps share the same app handler, it should be
  // make safe for ShutdownHandler to be called multiple times.
  virtual void ShutdownHandler() = 0;

  // Called when a GCM message has been received.
  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) = 0;

  // Called when some GCM messages have been deleted from the server.
  virtual void OnMessagesDeleted(const std::string& app_id) = 0;

  // Called when a GCM message failed to be delivered.
  virtual void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) = 0;
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_APP_HANDLER_H_
