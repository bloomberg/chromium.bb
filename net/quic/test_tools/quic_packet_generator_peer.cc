// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_packet_generator_peer.h"

#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_packet_generator.h"

namespace net {
namespace test {

// static
QuicPacketCreator* QuicPacketGeneratorPeer::GetPacketCreator(
    QuicPacketGenerator* generator) {
  return &generator->packet_creator_;
}

// static
QuicTime::Delta QuicPacketGeneratorPeer::GetFecTimeout(
    QuicPacketGenerator* generator) {
  return generator->fec_timeout_;
}

// static
FecSendPolicy QuicPacketGeneratorPeer::GetFecSendPolicy(
    QuicPacketGenerator* generator) {
  return generator->fec_send_policy_;
}

// static
void QuicPacketGeneratorPeer::SetFecSendPolicy(QuicPacketGenerator* generator,
                                               FecSendPolicy fec_send_policy) {
  generator->fec_send_policy_ = fec_send_policy;
}

}  // namespace test
}  // namespace net
