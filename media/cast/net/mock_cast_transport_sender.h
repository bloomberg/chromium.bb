// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_MOCK_CAST_TRANSPORT_SENDER_H_
#define MEDIA_CAST_NET_MOCK_CAST_TRANSPORT_SENDER_H_

#include <stdint.h>

#include "media/cast/net/cast_transport_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockCastTransportSender : public CastTransportSender {
 public:
  MockCastTransportSender();
  virtual ~MockCastTransportSender();

  MOCK_METHOD3(InitializeAudio, void(
      const CastTransportRtpConfig& config,
      const RtcpCastMessageCallback& cast_message_cb,
      const RtcpRttCallback& rtt_cb));
  MOCK_METHOD3(InitializeVideo, void(
      const CastTransportRtpConfig& config,
      const RtcpCastMessageCallback& cast_message_cb,
      const RtcpRttCallback& rtt_cb));
  MOCK_METHOD2(InsertFrame, void(uint32_t ssrc, const EncodedFrame& frame));
  MOCK_METHOD3(SendSenderReport,
               void(uint32_t ssrc,
                    base::TimeTicks current_time,
                    RtpTimeTicks current_time_as_rtp_timestamp));
  MOCK_METHOD2(CancelSendingFrames, void(
      uint32_t ssrc,
      const std::vector<uint32_t>& frame_ids));
  MOCK_METHOD2(ResendFrameForKickstart, void(uint32_t ssrc, uint32_t frame_id));
  MOCK_METHOD0(PacketReceiverForTesting, PacketReceiverCallback());
  MOCK_METHOD1(AddValidSsrc, void(uint32_t ssrc));
  MOCK_METHOD7(SendRtcpFromRtpReceiver,
               void(uint32_t ssrc,
                    uint32_t sender_ssrc,
                    const RtcpTimeData& time_data,
                    const RtcpCastMessage* cast_message,
                    base::TimeDelta target_delay,
                    const ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
                    const RtpReceiverStatistics* rtp_receiver_statistics));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_MOCK_CAST_TRANSPORT_SENDER_H_
