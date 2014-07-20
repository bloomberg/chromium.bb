// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_MOCK_RTCP_RECEIVER_FEEDBACK_H_
#define MEDIA_CAST_RTCP_MOCK_RTCP_RECEIVER_FEEDBACK_H_

#include <vector>

#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

// TODO(hclam): Should be renamed to MockRtcpMessageHandler.
class MockRtcpReceiverFeedback : public RtcpMessageHandler {
 public:
  MockRtcpReceiverFeedback();
  virtual ~MockRtcpReceiverFeedback();

  MOCK_METHOD1(OnReceivedSenderReport,
               void(const RtcpSenderInfo& remote_sender_info));

  MOCK_METHOD1(OnReceiverReferenceTimeReport,
               void(const RtcpReceiverReferenceTimeReport& remote_time_report));

  MOCK_METHOD0(OnReceivedSendReportRequest, void());

  MOCK_METHOD1(OnReceivedReceiverLog,
               void(const RtcpReceiverLogMessage& receiver_log));

  MOCK_METHOD2(OnReceivedDelaySinceLastReport,
               void(uint32 last_report, uint32 delay_since_last_report));

  MOCK_METHOD1(OnReceivedCastFeedback,
               void(const RtcpCastMessage& cast_message));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_MOCK_RTCP_RECEIVER_FEEDBACK_H_
