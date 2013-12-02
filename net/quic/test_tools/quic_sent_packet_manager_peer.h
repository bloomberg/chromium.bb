// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class QuicSentPacketManager;
class SendAlgorithmInterface;

namespace test {

class QuicSentPacketManagerPeer {
 public:
  static void SetSendAlgorithm(QuicSentPacketManager* sent_packet_manager,
                               SendAlgorithmInterface* send_algorithm);

  static size_t GetNackCount(
      const QuicSentPacketManager* sent_packet_manager,
      QuicPacketSequenceNumber sequence_number);

  static QuicTime::Delta rtt(QuicSentPacketManager* sent_packet_manager);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSentPacketManagerPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SENT_PACKET_MANAGER_PEER_H_
