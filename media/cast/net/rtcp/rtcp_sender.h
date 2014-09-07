// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_SENDER_H_
#define MEDIA_CAST_NET_RTCP_RTCP_SENDER_H_

#include <deque>
#include <list>
#include <string>

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

class PacedPacketSender;

// TODO(hclam): This should be renamed to RtcpPacketBuilder. The function
// of this class is to only to build a RTCP packet but not to send it.
class RtcpSender {
 public:
  RtcpSender(PacedPacketSender* outgoing_transport,
             uint32 sending_ssrc);
  ~RtcpSender();

  // TODO(hclam): This method should be to build a packet instead of
  // sending it.
  void SendRtcpFromRtpReceiver(
      const RtcpReportBlock* report_block,
      const RtcpReceiverReferenceTimeReport* rrtr,
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
      base::TimeDelta target_delay);

  // TODO(hclam): This method should be to build a packet instead of
  // sending it.
  void SendRtcpFromRtpSender(const RtcpSenderInfo& sender_info);

 private:
  void BuildRR(const RtcpReportBlock* report_block,
               Packet* packet) const;

  void AddReportBlocks(const RtcpReportBlock& report_block,
                       Packet* packet) const;

  void BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                 Packet* packet) const;

  void BuildCast(const RtcpCastMessage* cast_message,
                 base::TimeDelta target_delay,
                 Packet* packet) const;

  void BuildSR(const RtcpSenderInfo& sender_info, Packet* packet) const;

  void BuildDlrrRb(const RtcpDlrrReportBlock& dlrr, Packet* packet) const;

  void BuildReceiverLog(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      Packet* packet);

  bool BuildRtcpReceiverLogMessage(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      size_t start_size,
      RtcpReceiverLogMessage* receiver_log_message,
      size_t* number_of_frames,
      size_t* total_number_of_messages_to_send,
      size_t* rtcp_log_size);

  const uint32 ssrc_;

 // Not owned by this class.
  PacedPacketSender* const transport_;

  std::deque<RtcpReceiverLogMessage> rtcp_events_history_;

  DISALLOW_COPY_AND_ASSIGN(RtcpSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_SENDER_H_
