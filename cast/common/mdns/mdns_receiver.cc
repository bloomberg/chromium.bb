// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_receiver.h"

#include "cast/common/mdns/mdns_reader.h"
#include "platform/api/trace_logging.h"

namespace cast {
namespace mdns {

MdnsReceiver::MdnsReceiver(UdpSocket* socket,
                           NetworkRunner* network_runner,
                           Delegate* delegate)
    : socket_(socket), network_runner_(network_runner), delegate_(delegate) {
  OSP_DCHECK(socket_);
  OSP_DCHECK(network_runner_);
  OSP_DCHECK(delegate_);
}

MdnsReceiver::~MdnsReceiver() {
  if (state_ == State::kRunning) {
    Stop();
  }
}

Error MdnsReceiver::Start() {
  if (state_ == State::kRunning) {
    return Error::Code::kNone;
  }
  Error result = network_runner_->ReadRepeatedly(socket_, this);
  if (result.ok()) {
    state_ = State::kRunning;
  }
  return result;
}

Error MdnsReceiver::Stop() {
  if (state_ == State::kStopped) {
    return Error::Code::kNone;
  }
  Error result = network_runner_->CancelRead(socket_);
  if (result.ok()) {
    state_ = State::kStopped;
  }
  return result;
}

void MdnsReceiver::OnRead(UdpPacket packet, NetworkRunner* network_runner) {
  TRACE_SCOPED(TraceCategory::mDNS, "MdnsReceiver::OnRead");
  MdnsReader reader(packet.data(), packet.size());
  MdnsMessage message;
  if (!reader.Read(&message)) {
    return;
  }
  if (message.type() == MessageType::Response) {
    delegate_->OnResponseReceived(message, packet.source());
  } else {
    delegate_->OnQueryReceived(message, packet.source());
  }
}

}  // namespace mdns
}  // namespace cast
