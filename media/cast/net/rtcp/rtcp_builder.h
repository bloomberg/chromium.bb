// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_BUILDER_H_
#define MEDIA_CAST_NET_RTCP_RTCP_BUILDER_H_

#include <deque>
#include <list>
#include <string>

#include "base/big_endian.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

// We limit the size of receiver logs to avoid queuing up packets.
const size_t kMaxReceiverLogBytes = 200;

// The determines how long to hold receiver log events, based on how
// many "receiver log message reports" ago the events were sent.
const size_t kReceiveLogMessageHistorySize = 20;

// This determines when to send events the second time.
const size_t kFirstRedundancyOffset = 10;
COMPILE_ASSERT(kFirstRedundancyOffset > 0 &&
                   kFirstRedundancyOffset <= kReceiveLogMessageHistorySize,
               redundancy_offset_out_of_range);

// When to send events the third time.
const size_t kSecondRedundancyOffset = 20;
COMPILE_ASSERT(kSecondRedundancyOffset >
                   kFirstRedundancyOffset && kSecondRedundancyOffset <=
                   kReceiveLogMessageHistorySize,
               redundancy_offset_out_of_range);


class RtcpBuilder {
 public:
  explicit RtcpBuilder(uint32 sending_ssrc);
  ~RtcpBuilder();

  PacketRef BuildRtcpFromReceiver(
      const RtcpReportBlock* report_block,
      const RtcpReceiverReferenceTimeReport* rrtr,
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
      base::TimeDelta target_delay);

  PacketRef BuildRtcpFromSender(const RtcpSenderInfo& sender_info);

 private:
  void AddRtcpHeader(RtcpPacketFields payload, int format_or_count);
  void PatchLengthField();
  void AddRR(const RtcpReportBlock* report_block);
  void AddReportBlocks(const RtcpReportBlock& report_block);
  void AddRrtr(const RtcpReceiverReferenceTimeReport* rrtr);
  void AddCast(const RtcpCastMessage* cast_message,
               base::TimeDelta target_delay);
  void AddSR(const RtcpSenderInfo& sender_info);
  void AddDlrrRb(const RtcpDlrrReportBlock& dlrr);
  void AddReceiverLog(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events);

  bool GetRtcpReceiverLogMessage(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      RtcpReceiverLogMessage* receiver_log_message,
      size_t* total_number_of_messages_to_send);

  void Start();
  PacketRef Finish();

  base::BigEndianWriter writer_;
  const uint32 ssrc_;
  char* ptr_of_length_;
  PacketRef packet_;
  std::deque<RtcpReceiverLogMessage> rtcp_events_history_;

  DISALLOW_COPY_AND_ASSIGN(RtcpBuilder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_SENDER_H_
