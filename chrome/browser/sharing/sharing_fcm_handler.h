// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {
class GCMDriver;
}

class SharingMessageHandler;
class SharingFCMSender;

// SharingFCMHandler is responsible for receiving SharingMessage from GCMDriver
// and delegate it to the payload specific handler.
class SharingFCMHandler : public gcm::GCMAppHandler {
  using SharingMessage = chrome_browser_sharing::SharingMessage;

 public:
  SharingFCMHandler(gcm::GCMDriver* gcm_driver,
                    SharingFCMSender* sharing_fcm_sender);
  ~SharingFCMHandler() override;

  // Registers itself as app handler for sharing messages.
  virtual void StartListening();

  // Unregisters itself as app handler for sharing messages.
  virtual void StopListening();

  // Registers |handler| for handling |payload_case| SharingMessage.
  virtual void AddSharingHandler(
      const SharingMessage::PayloadCase& payload_case,
      SharingMessageHandler* handler);

  // Removes SharingMessageHandler registered for |payload_case|.
  virtual void RemoveSharingHandler(
      const SharingMessage::PayloadCase& payload_case);

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;

  // Responsible for delegating the message to the registered
  // SharingMessageHandler. Also sends back an ack to original sender after
  // delegating the message.
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;

  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;

  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  void OnMessagesDeleted(const std::string& app_id) override;

 private:
  // Ack message sent back to the original sender of message.
  void SendAckMessage(const std::string& original_sender_guid,
                      const std::string& original_message_id);

  void OnAckMessageSent(const std::string& original_message_id,
                        base::Optional<std::string> message_id);

  gcm::GCMDriver* const gcm_driver_;
  SharingFCMSender* sharing_fcm_sender_;

  std::map<SharingMessage::PayloadCase, SharingMessageHandler*>
      sharing_handlers_;
  bool is_listening_ = false;

  base::WeakPtrFactory<SharingFCMHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharingFCMHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_FCM_HANDLER_H_
