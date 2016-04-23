// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_SENDER_RTCP_SESSION_H_
#define MEDIA_CAST_NET_RTCP_SENDER_RTCP_SESSION_H_

#include <map>
#include <queue>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/time/time.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_session.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

typedef std::pair<uint32_t, base::TimeTicks> RtcpSendTimePair;
typedef std::map<uint32_t, base::TimeTicks> RtcpSendTimeMap;
typedef std::queue<RtcpSendTimePair> RtcpSendTimeQueue;

// This class represents a RTCP session on a RTP sender. It provides an
// interface to send RTCP sender report (SR). RTCP SR packets allow
// receiver to maintain clock offsets and synchronize between audio and video.
//
// RTCP session on sender handles the following incoming RTCP reports
// from receiver:
// - Receiver reference time report: Helps with tracking largest timestamp
//   seen and as a result rejecting old RTCP reports.
// - Receiver logs: The sender receives log events from the receiver and
//   invokes a callback passed.
// - cast message: Receives feedback from receiver on missing packets/frames
//   and last frame id received and invokes a callback passed.
// - Last report: The receiver provides feedback on delay since last report
//   received which helps it compute round trip time and invoke a callback.
class SenderRtcpSession : public RtcpSession {
 public:
  // TODO(xjz): Simplify the interface. http://crbug.com/588275.
  SenderRtcpSession(const RtcpCastMessageCallback& cast_callback,
                    const RtcpRttCallback& rtt_callback,
                    const RtcpLogMessageCallback& log_callback,
                    const RtcpPliCallback pli_callback,
                    base::TickClock* clock,            // Not owned.
                    PacedPacketSender* packet_sender,  // Not owned.
                    uint32_t local_ssrc,
                    uint32_t remote_ssrc);

  ~SenderRtcpSession() override;

  // If greater than zero, this is the last measured network round trip time.
  base::TimeDelta current_round_trip_time() const {
    return current_round_trip_time_;
  }

  // Accounts for the fact that a frame with |frame_id| is being sent to the
  // receiver.  This is used so that the parser of the RTCP messages coming back
  // from the receiver will not interpret the truncated frame IDs from very old
  // packets as coming "from the future."
  void WillSendFrame(FrameId frame_id);

  // Send a RTCP sender report.
  // |current_time| is the current time reported by a tick clock.
  // |current_time_as_rtp_timestamp| is the corresponding RTP timestamp.
  // |send_packet_count| is the number of packets sent.
  // |send_octet_count| is the number of octets sent.
  void SendRtcpReport(base::TimeTicks current_time,
                      RtpTimeTicks current_time_as_rtp_timestamp,
                      uint32_t send_packet_count,
                      size_t send_octet_count);

  // Handle incoming RTCP packet.
  // Returns false if it is not a RTCP packet or it is not directed to
  // this session, e.g. SSRC doesn't match.
  bool IncomingRtcpPacket(const uint8_t* data, size_t length) override;

 private:
  // Received last report information from RTP receiver which helps compute
  // round trip time.
  void OnReceivedDelaySinceLastReport(uint32_t last_report,
                                      uint32_t delay_since_last_report);

  // Received logs from RTP receiver.
  void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log);

  // Received cast message containing details of missing packets/frames and
  // last frame id received.
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_message);

  // Remove duplicate events in |receiver_log|.
  // Returns true if any events remain.
  bool DedupeReceiverLog(RtcpReceiverLogMessage* receiver_log);

  // Save last sent NTP time on RTPC SR. This helps map the sent time when a
  // last report is received from RTP receiver to compute RTT.
  void SaveLastSentNtpTime(const base::TimeTicks& now,
                           uint32_t last_ntp_seconds,
                           uint32_t last_ntp_fraction);

  base::TickClock* const clock_;      // Not owned.
  PacedPacketSender* packet_sender_;  // Not owned.
  const uint32_t local_ssrc_;
  const uint32_t remote_ssrc_;

  const RtcpCastMessageCallback cast_callback_;
  const RtcpRttCallback rtt_callback_;
  const RtcpLogMessageCallback log_callback_;
  const RtcpPliCallback pli_callback_;

  // Computed from RTCP RRTR report.
  base::TimeTicks largest_seen_timestamp_;

  // The RTCP packet parser is re-used when parsing each RTCP packet.  It
  // remembers state about prior RTP timestamps and other sequence values to
  // re-construct "expanded" values.
  RtcpParser parser_;

  // Maintains a history of receiver events.
  typedef std::pair<uint64_t, uint64_t> ReceiverEventKey;
  base::hash_set<ReceiverEventKey> receiver_event_key_set_;
  std::queue<ReceiverEventKey> receiver_event_key_queue_;

  // The last measured network round trip time.  This is updated with each
  // sender report --> receiver report round trip.  If this is zero, then the
  // round trip time has not been measured yet.
  base::TimeDelta current_round_trip_time_;

  // Map of NTP timestamp to local time that helps with RTT calculation
  // when last report is received from RTP receiver.
  RtcpSendTimeMap last_reports_sent_map_;
  RtcpSendTimeQueue last_reports_sent_queue_;

  DISALLOW_COPY_AND_ASSIGN(SenderRtcpSession);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_SENDER_RTCP_SESSION_H_
