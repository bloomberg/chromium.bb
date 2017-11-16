// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/fake_connection.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/cryptauth/wire_message.h"

namespace cryptauth {

FakeConnection::FakeConnection(const RemoteDevice& remote_device)
    : FakeConnection(remote_device, /* should_auto_connect */ true) {}

FakeConnection::FakeConnection(
    const RemoteDevice& remote_device, bool should_auto_connect)
    : Connection(remote_device), should_auto_connect_(should_auto_connect) {
  if (should_auto_connect_) {
    Connect();
  }
}

FakeConnection::~FakeConnection() {
  Disconnect();
}

void FakeConnection::Connect() {
  if (should_auto_connect_) {
    SetStatus(CONNECTED);
  } else {
    SetStatus(IN_PROGRESS);
  }
}

void FakeConnection::Disconnect() {
  SetStatus(DISCONNECTED);
}

void FakeConnection::AddObserver(ConnectionObserver* observer) {
  observers_.push_back(observer);
  Connection::AddObserver(observer);
}

void FakeConnection::RemoveObserver(ConnectionObserver* observer) {
  observers_.erase(
      std::remove(observers_.begin(), observers_.end(), observer),
      observers_.end());
  Connection::RemoveObserver(observer);
}

void FakeConnection::CompleteInProgressConnection(bool success) {
  DCHECK(!should_auto_connect_);
  DCHECK(status() == IN_PROGRESS);

  if (success) {
    SetStatus(CONNECTED);
  } else {
    SetStatus(DISCONNECTED);
  }
}

void FakeConnection::FinishSendingMessageWithSuccess(bool success) {
  CHECK(current_message_);
  // Capture a copy of the message, as OnDidSendMessage() might reentrantly
  // call SendMessage().
  std::unique_ptr<WireMessage> sent_message = std::move(current_message_);
  OnDidSendMessage(*sent_message, success);
}

void FakeConnection::ReceiveMessage(
    const std::string& feature, const std::string& payload) {
  pending_feature_ = feature;
  pending_payload_ = payload;
  OnBytesReceived(std::string());
  pending_feature_.clear();
  pending_payload_.clear();
}

void FakeConnection::NotifyGattCharacteristicsNotAvailable() {
  Connection::NotifyGattCharacteristicsNotAvailable();
}

void FakeConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  CHECK(!current_message_);
  current_message_ = std::move(message);
}

std::unique_ptr<WireMessage> FakeConnection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  *is_incomplete_message = false;
  return base::MakeUnique<WireMessage>(pending_payload_, pending_feature_);
}

}  // namespace cryptauth
