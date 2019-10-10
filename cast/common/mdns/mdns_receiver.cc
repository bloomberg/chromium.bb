// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "cast/common/mdns/mdns_reader.h"
#include "platform/api/trace_logging.h"

namespace cast {
namespace mdns {

MdnsReceiver::MdnsReceiver(UdpSocket* socket) : socket_(socket) {
  OSP_DCHECK(socket_);
}

MdnsReceiver::~MdnsReceiver() {
  if (state_ == State::kRunning) {
    Stop();
  }
}

void MdnsReceiver::SetQueryCallback(
    std::function<void(const MdnsMessage&)> callback) {
  // This check verifies that either new or stored callback has a target. It
  // will fail in case multiple objects try to set or clear the callback.
  OSP_DCHECK(static_cast<bool>(query_callback_) != static_cast<bool>(callback));
  query_callback_ = callback;
}

void MdnsReceiver::SetResponseCallback(
    std::function<void(const MdnsMessage&)> callback) {
  // This check verifies that either new or stored callback has a target. It
  // will fail in case multiple objects try to set or clear the callback.
  OSP_DCHECK(static_cast<bool>(response_callback_) !=
             static_cast<bool>(callback));
  response_callback_ = callback;
}

void MdnsReceiver::Start() {
  state_ = State::kRunning;
}

void MdnsReceiver::Stop() {
  state_ = State::kStopped;
}

void MdnsReceiver::OnRead(UdpSocket* socket,
                          ErrorOr<UdpPacket> packet_or_error) {
  if (state_ != State::kRunning || packet_or_error.is_error()) {
    return;
  }

  UdpPacket packet = std::move(packet_or_error.value());

  TRACE_SCOPED(TraceCategory::mDNS, "MdnsReceiver::OnRead");
  MdnsReader reader(packet.data(), packet.size());
  MdnsMessage message;
  if (!reader.Read(&message)) {
    return;
  }

  std::function<void(const MdnsMessage&)> callback =
      (message.type() == MessageType::Response) ? response_callback_
                                                : query_callback_;

  if (callback) {
    callback(message);
  }
}

void MdnsReceiver::OnError(UdpSocket* socket, Error error) {
  // This method should never be called for MdnsReciever.
  OSP_UNIMPLEMENTED();
}

void MdnsReceiver::OnSendError(UdpSocket* socket, Error error) {
  // This method should never be called for MdnsReciever.
  OSP_UNIMPLEMENTED();
}

}  // namespace mdns
}  // namespace cast
