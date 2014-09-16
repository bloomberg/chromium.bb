// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/connection.h"

#include "base/logging.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/wire_message.h"

namespace proximity_auth {

Connection::Connection(const RemoteDevice& remote_device)
    : remote_device_(remote_device),
      status_(DISCONNECTED),
      is_sending_message_(false) {
}

Connection::~Connection() {
}

bool Connection::IsConnected() const {
  return status_ == CONNECTED;
}

void Connection::SendMessage(scoped_ptr<WireMessage> message) {
  if (!IsConnected()) {
    VLOG(1) << "Cannot send message when disconnected.";
    return;
  }

  if (is_sending_message_) {
    VLOG(1) << "Another message is currently in progress.";
    return;
  }

  is_sending_message_ = true;
  SendMessageImpl(message.Pass());
}

void Connection::AddObserver(ConnectionObserver* observer) {
  observers_.AddObserver(observer);
}

void Connection::RemoveObserver(ConnectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Connection::SetStatus(Status status) {
  if (status_ == status)
    return;

  received_bytes_.clear();

  Status old_status = status_;
  status_ = status;
  FOR_EACH_OBSERVER(ConnectionObserver,
                    observers_,
                    OnConnectionStatusChanged(*this, old_status, status_));
}

void Connection::OnDidSendMessage(const WireMessage& message, bool success) {
  if (!is_sending_message_) {
    VLOG(1) << "Send completed, but no message in progress.";
    return;
  }

  is_sending_message_ = false;
  FOR_EACH_OBSERVER(
      ConnectionObserver, observers_, OnSendCompleted(*this, message, success));
}

void Connection::OnBytesReceived(const std::string& bytes) {
  if (!IsConnected()) {
    VLOG(1) << "Received bytes, but not connected.";
    return;
  }

  received_bytes_ += bytes;

  bool is_incomplete_message;
  scoped_ptr<WireMessage> message =
      DeserializeWireMessage(&is_incomplete_message);
  if (is_incomplete_message)
    return;

  if (message) {
    FOR_EACH_OBSERVER(
        ConnectionObserver, observers_, OnMessageReceived(*this, *message));
  }

  // Whether the message was parsed successfully or not, clear the
  // |received_bytes_| buffer.
  received_bytes_.clear();
}

scoped_ptr<WireMessage> Connection::DeserializeWireMessage(
    bool* is_incomplete_message) {
  return WireMessage::Deserialize(received_bytes_, is_incomplete_message);
}

}  // namespace proximity_auth
