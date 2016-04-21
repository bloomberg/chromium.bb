// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_FAKE_CONNECTION_H
#define COMPONENTS_PROXIMITY_AUTH_FAKE_CONNECTION_H

#include "base/macros.h"
#include "components/proximity_auth/connection.h"

namespace proximity_auth {

// A fake implementation of Connection to use in tests.
class FakeConnection : public Connection {
 public:
  FakeConnection(const RemoteDevice& remote_device);
  ~FakeConnection() override;

  // Connection:
  void Connect() override;
  void Disconnect() override;

  // Completes the current send operation with success |success|.
  void FinishSendingMessageWithSuccess(bool success);

  // Simulates receiving a wire message with the given |payload|, bypassing the
  // container WireMessage format.
  void ReceiveMessageWithPayload(const std::string& payload);

  // Returns the current message in progress of being sent.
  WireMessage* current_message() { return current_message_.get(); }

  using Connection::SetStatus;

 private:
  // Connection:
  void SendMessageImpl(std::unique_ptr<WireMessage> message) override;
  std::unique_ptr<WireMessage> DeserializeWireMessage(
      bool* is_incomplete_message) override;

  // The message currently being sent. Only set between a call to
  // SendMessageImpl() and FinishSendingMessageWithSuccess().
  std::unique_ptr<WireMessage> current_message_;

  // The payload that should be returned when DeserializeWireMessage() is
  // called.
  std::string pending_payload_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnection);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_FAKE_CONNECTION_H
