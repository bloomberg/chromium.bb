// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/connection.h"

#include <sstream>
#include <utility>

#include "base/logging.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/cryptauth/connection_observer.h"
#include "components/cryptauth/wire_message.h"

namespace cryptauth {

Connection::Connection(const RemoteDevice& remote_device)
    : remote_device_(remote_device),
      status_(DISCONNECTED),
      is_sending_message_(false) {}

Connection::~Connection() {}

bool Connection::IsConnected() const {
  return status_ == CONNECTED;
}

void Connection::SendMessage(std::unique_ptr<WireMessage> message) {
  if (!IsConnected()) {
    PA_LOG(ERROR) << "Not yet connected; cannot send message to "
                  << GetDeviceInfoLogString();
    return;
  }

  if (is_sending_message_) {
    PA_LOG(ERROR) << "Cannot send message because another message is currently "
                  << "being sent to " << GetDeviceInfoLogString();
    return;
  }

  is_sending_message_ = true;
  SendMessageImpl(std::move(message));
}

void Connection::AddObserver(ConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void Connection::RemoveObserver(ConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::string Connection::GetDeviceAddress() {
  return remote_device_.bluetooth_address;
}

void Connection::SetStatus(Status status) {
  if (status_ == status)
    return;

  received_bytes_.clear();

  Status old_status = status_;
  status_ = status;
  for (auto& observer : observers_)
    observer.OnConnectionStatusChanged(this, old_status, status_);
}

void Connection::OnDidSendMessage(const WireMessage& message, bool success) {
  if (!is_sending_message_) {
    PA_LOG(ERROR) << "OnDidSendMessage(), but a send was not expected to be in "
                  << "progress to " << GetDeviceInfoLogString();
    return;
  }

  is_sending_message_ = false;
  for (auto& observer : observers_)
    observer.OnSendCompleted(*this, message, success);
}

void Connection::OnBytesReceived(const std::string& bytes) {
  if (!IsConnected()) {
    PA_LOG(ERROR) << "OnBytesReceived(), but not connected to "
                  << GetDeviceInfoLogString();
    return;
  }

  received_bytes_ += bytes;

  bool is_incomplete_message;
  std::unique_ptr<WireMessage> message =
      DeserializeWireMessage(&is_incomplete_message);
  if (is_incomplete_message)
    return;

  if (message) {
    for (auto& observer : observers_)
      observer.OnMessageReceived(*this, *message);
  }

  // Whether the message was parsed successfully or not, clear the
  // |received_bytes_| buffer.
  received_bytes_.clear();
}

std::unique_ptr<WireMessage> Connection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  return WireMessage::Deserialize(received_bytes_, is_incomplete_message);
}

void Connection::NotifyGattCharacteristicsNotAvailable() {
  for (auto& observer : observers_)
    observer.OnGattCharacteristicsNotAvailable();
}

std::string Connection::GetDeviceInfoLogString() {
  std::stringstream ss;
  ss << "{id: \"" << remote_device().GetTruncatedDeviceIdForLogs()
     << "\", addr: \"" << GetDeviceAddress() << "\"}";
  return ss.str();
}

}  // namespace cryptauth
