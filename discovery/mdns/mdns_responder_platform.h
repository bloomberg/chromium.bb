// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_PLATFORM_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_PLATFORM_H_

#include <vector>

#include "platform/api/socket.h"

struct mDNS_PlatformSupport_struct {
  std::vector<openscreen::platform::UdpSocketIPv4Ptr> v4_sockets;
  std::vector<openscreen::platform::UdpSocketIPv6Ptr> v6_sockets;
};

#endif  // DISCOVERY_MDNS_MDNS_RESPONDER_PLATFORM_H_
