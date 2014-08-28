// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_APP_HANDLER_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_APP_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace base {
class RunLoop;
}

namespace gcm {

class FakeGCMAppHandler : public GCMAppHandler {
 public:
  enum Event {
    NO_EVENT,
    MESSAGE_EVENT,
    MESSAGES_DELETED_EVENT,
    SEND_ERROR_EVENT
  };

  FakeGCMAppHandler();
  virtual ~FakeGCMAppHandler();

  const Event& received_event() const { return received_event_; }
  const std::string& app_id() const { return app_id_; }
  const std::string& acked_message_id() const { return acked_message_id_; }
  const GCMClient::IncomingMessage& message() const { return message_; }
  const GCMClient::SendErrorDetails& send_error_details() const {
    return send_error_details_;
  }

  void WaitForNotification();

  // GCMAppHandler implementation.
  virtual void ShutdownHandler() OVERRIDE;
  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE;
  virtual void OnSendAcknowledged(const std::string& app_id,
                                  const std::string& message_id) OVERRIDE;

 private:
  void ClearResults();

  scoped_ptr<base::RunLoop> run_loop_;

  Event received_event_;
  std::string app_id_;
  std::string acked_message_id_;
  GCMClient::IncomingMessage message_;
  GCMClient::SendErrorDetails send_error_details_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMAppHandler);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_APP_HANDLER_H_
