// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_PACKET_GENERATOR_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_PACKET_GENERATOR_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class QuicPacketGenerator;

namespace test {

class QuicPacketGeneratorPeer {
 public:
  static void MaybeStartFecProtection(QuicPacketGenerator* generator);

  static void MaybeStopFecProtection(QuicPacketGenerator* generator,
                                     bool force);

  // Convenience method for connection and above. Enables and turns on FEC
  // protection in the generator.
  static void SwitchFecProtectionOn(QuicPacketGenerator* generator,
                                    size_t max_packets_per_fec_group);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicPacketGeneratorPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_PACKET_GENERATOR_PEER_H_
