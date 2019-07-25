// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_RECEIVER_H_
#define CAST_COMMON_MDNS_MDNS_RECEIVER_H_

#include "cast/common/mdns/mdns_records.h"
#include "platform/api/network_runner.h"
#include "platform/api/udp_packet.h"
#include "platform/api/udp_read_callback.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace cast {
namespace mdns {

using Error = openscreen::Error;
using NetworkRunner = openscreen::platform::NetworkRunner;
using UdpSocket = openscreen::platform::UdpSocket;
using UdpReadCallback = openscreen::platform::UdpReadCallback;
using UdpPacket = openscreen::platform::UdpPacket;
using IPEndpoint = openscreen::IPEndpoint;

class MdnsReceiver : UdpReadCallback {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnQueryReceived(const MdnsMessage& message,
                                 const IPEndpoint& sender) = 0;
    virtual void OnResponseReceived(const MdnsMessage& message,
                                    const IPEndpoint& sender) = 0;
  };

  // MdnsReceiver does not own |socket|, |network_runner| and |delegate|
  // and expects that the lifetime of these objects exceeds the lifetime of
  // MdnsReceiver.
  MdnsReceiver(UdpSocket* socket,
               NetworkRunner* network_runner,
               Delegate* delegate);
  MdnsReceiver(const MdnsReceiver& other) = delete;
  MdnsReceiver(MdnsReceiver&& other) noexcept = delete;
  ~MdnsReceiver() override;

  MdnsReceiver& operator=(const MdnsReceiver& other) = delete;
  MdnsReceiver& operator=(MdnsReceiver&& other) noexcept = delete;

  // The receiver can be started and stopped multiple times.
  // Start and Stop return Error::Code::kNone on success and return an error on
  // failure. Start returns Error::Code::kNone when called on a receiver that
  // has already been started. Stop returns Error::Code::kNone when called on a
  // receiver that has already been stopped or not yet started. Start and Stop
  // are both synchronous calls. After MdnsReceiver has been started it will
  // receive OnRead callbacks from the network runner.
  Error Start();
  Error Stop();

  void OnRead(UdpPacket packet, NetworkRunner* network_runner) override;

 private:
  enum class State {
    kStopped,
    kRunning,
  };

  UdpSocket* const socket_;
  NetworkRunner* const network_runner_;
  Delegate* const delegate_;
  State state_ = State::kStopped;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_RECEIVER_H_
