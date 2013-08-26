// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_PACING_MOCK_PACED_PACKET_SENDER_H_
#define MEDIA_CAST_PACING_MOCK_PACED_PACKET_SENDER_H_

#include "media/cast/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockPacedPacketSender : public PacedPacketSender {
 public:
  MOCK_METHOD2(SendPacket,
               bool(const std::vector<uint8>& packet, int num_of_packets));
  MOCK_METHOD2(ResendPacket,
               bool(const std::vector<uint8>& packet, int num_of_packets));
  MOCK_METHOD1(SendRtcpPacket, bool(const std::vector<uint8>& packet));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_PACING_MOCK_PACED_PACKET_SENDER_H_
