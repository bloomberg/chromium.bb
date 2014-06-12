// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_packet_generator_peer.h"

#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_packet_generator.h"

namespace net {
namespace test {

// static
void QuicPacketGeneratorPeer::MaybeStartFecProtection(
    QuicPacketGenerator* generator) {
  generator->MaybeStartFecProtection();
}

// static
void QuicPacketGeneratorPeer::MaybeStopFecProtection(
    QuicPacketGenerator* generator, bool force) {
  generator->MaybeStopFecProtection(force);
}

// static
void QuicPacketGeneratorPeer::SwitchFecProtectionOn(
  QuicPacketGenerator* generator, size_t max_packets_per_fec_group) {
  generator->packet_creator_->set_max_packets_per_fec_group(
      max_packets_per_fec_group);
  generator->MaybeStartFecProtection();
}

}  // namespace test
}  // namespace net
