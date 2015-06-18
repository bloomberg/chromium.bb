// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_ACK_NOTIFIER_MANAGER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_ACK_NOTIFIER_MANAGER_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class AckNotifierManager;

namespace test {

// Exposes the internal fields of AckNotifierManager for tests.
class AckNotifierManagerPeer {
 public:
  // Returns total number of packets known to the map.
  static size_t GetNumberOfRegisteredPackets(const AckNotifierManager* manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(AckNotifierManagerPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_ACK_NOTIFIER_MANAGER_PEER_H_
