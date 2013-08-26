// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_PACING_MOCK_PACKET_SENDER_H_
#define MEDIA_CAST_PACING_MOCK_PACKET_SENDER_H_

#include "media/cast/cast_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockPacketSender : public PacketSender {
 public:
  MOCK_METHOD2(SendPacket, bool(const uint8* packet, int length));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_PACING_MOCK_PACKET_SENDER_H_
