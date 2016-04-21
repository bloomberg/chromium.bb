// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/fake_connection.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {

FakeConnection::FakeConnection(const RemoteDevice& remote_device)
    : Connection(remote_device) {
  Connect();
}

FakeConnection::~FakeConnection() {
  Disconnect();
}

void FakeConnection::Connect() {
  SetStatus(CONNECTED);
}

void FakeConnection::Disconnect() {
  SetStatus(DISCONNECTED);
}

void FakeConnection::FinishSendingMessageWithSuccess(bool success) {
  CHECK(current_message_);
  // Capture a copy of the message, as OnDidSendMessage() might reentrantly
  // call SendMessage().
  std::unique_ptr<WireMessage> sent_message = std::move(current_message_);
  OnDidSendMessage(*sent_message, success);
}

void FakeConnection::ReceiveMessageWithPayload(const std::string& payload) {
  pending_payload_ = payload;
  OnBytesReceived(std::string());
  pending_payload_.clear();
}

void FakeConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  CHECK(!current_message_);
  current_message_ = std::move(message);
}

std::unique_ptr<WireMessage> FakeConnection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  *is_incomplete_message = false;
  return base::WrapUnique(new WireMessage(pending_payload_));
}

}  // namespace proximity_auth
