// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_framer_peer.h"

#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {
namespace test {

// static
QuicPacketSequenceNumber QuicFramerPeer::CalculatePacketSequenceNumberFromWire(
    QuicFramer* framer,
    QuicPacketSequenceNumber packet_sequence_number) {
  return framer->CalculatePacketSequenceNumberFromWire(packet_sequence_number);
}

// static
void QuicFramerPeer::SetLastSequenceNumber(
    QuicFramer* framer,
    QuicPacketSequenceNumber packet_sequence_number) {
  framer->last_sequence_number_ = packet_sequence_number;
}

void QuicFramerPeer::SetIsServer(QuicFramer* framer, bool is_server) {
  framer->is_server_ = is_server;
}

void QuicFramerPeer::SetVersion(QuicFramer* framer, QuicVersionTag version) {
  framer->quic_version_ = version;
}

}  // namespace test
}  // namespace net
