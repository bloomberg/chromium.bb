// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_SENDER_H_
#define CAST_COMMON_MDNS_MDNS_SENDER_H_

#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace cast {
namespace mdns {

class MdnsMessage;

class MdnsSender {
 public:
  using Error = openscreen::Error;
  using IPEndpoint = openscreen::IPEndpoint;
  using UdpSocket = openscreen::platform::UdpSocket;

  // MdnsSender does not own |socket| and expects that its lifetime exceeds the
  // lifetime of MdnsSender.
  explicit MdnsSender(UdpSocket* socket);
  MdnsSender(const MdnsSender& other) = delete;
  MdnsSender(MdnsSender&& other) noexcept = delete;
  MdnsSender& operator=(const MdnsSender& other) = delete;
  MdnsSender& operator=(MdnsSender&& other) noexcept = delete;
  ~MdnsSender() = default;

  Error SendMulticast(const MdnsMessage& message);
  Error SendUnicast(const MdnsMessage& message, const IPEndpoint& endpoint);

 private:
  UdpSocket* const socket_;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_SENDER_H_
