// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_DEFAULT_GCM_APP_HANDLER_H_
#define COMPONENTS_GCM_DRIVER_DEFAULT_GCM_APP_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

// The default app handler that is triggered when there is no registered app
// handler for an application id.
class DefaultGCMAppHandler : public GCMAppHandler {
 public:
  DefaultGCMAppHandler();
  ~DefaultGCMAppHandler() override;

  // Overridden from GCMAppHandler:
  void ShutdownHandler() override;
  void OnMessage(const std::string& app_id,
                 const IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultGCMAppHandler);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_DEFAULT_GCM_APP_HANDLER_H_
