// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_PACING_MOCK_PACED_PACKET_SENDER_H_
#define MEDIA_CAST_TRANSPORT_PACING_MOCK_PACED_PACKET_SENDER_H_

#include "media/cast/transport/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {
namespace transport {

class MockPacedPacketSender : public PacedPacketSender {
 public:
  MockPacedPacketSender();
  virtual ~MockPacedPacketSender();

  MOCK_METHOD1(SendPackets, bool(const PacketList& packets));
  MOCK_METHOD1(ResendPackets, bool(const PacketList& packets));
  MOCK_METHOD1(SendRtcpPacket, bool(PacketRef packet));
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_PACING_MOCK_PACED_PACKET_SENDER_H_
