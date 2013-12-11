// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_SENDER_MOCK_RTP_SENDER_H_
#define MEDIA_CAST_RTP_SENDER_MOCK_RTP_SENDER_H_

#include <vector>

#include "media/cast/net/rtp_sender/rtp_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockRtpSender : public RtpSender {
 public:
  MOCK_METHOD2(IncomingEncodedVideoFrame,
               bool(const EncodedVideoFrame& frame, int64 capture_time));

  MOCK_METHOD2(IncomingEncodedAudioFrame,
               bool(const EncodedAudioFrame& frame, int64 recorded_time));

  MOCK_METHOD3(ResendPacket,
               bool(bool is_audio, uint32 frame_id, uint16 packet_id));

  MOCK_METHOD0(RtpStatistics, void());
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_SENDER_MOCK_RTP_SENDER_H_

