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
  MOCK_METHOD2(AddValidRtpReceiver,
               void(uint32_t rtp_sender_ssrc, uint32_t rtp_receiver_ssrc));
  MOCK_METHOD2(InitializeRtpReceiverRtcpBuilder,
               void(uint32_t rtp_receiver_ssrc, const RtcpTimeData& time_data));
  MOCK_METHOD2(AddCastFeedback,
               void(const RtcpCastMessage& cast_message,
                    base::TimeDelta target_delay));
  MOCK_METHOD1(
      AddRtcpEvents,
      void(const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events));
  MOCK_METHOD1(AddRtpReceiverReport,
               void(const RtcpReportBlock& rtp_report_block));
  MOCK_METHOD0(SendRtcpFromRtpReceiver, void());
  MOCK_METHOD1(SetOptions, void(const base::DictionaryValue& options));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_MOCK_CAST_TRANSPORT_SENDER_H_
