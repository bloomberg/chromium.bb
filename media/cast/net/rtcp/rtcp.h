// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class maintains a bi-directional RTCP connection with a remote
// peer.

#ifndef MEDIA_CAST_RTCP_RTCP_H_
#define MEDIA_CAST_RTCP_RTCP_H_

#include <map>
#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/common/clock_drift_smoother.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

class LocalRtcpReceiverFeedback;
class PacedPacketSender;
class RtcpReceiver;
class RtcpSender;

typedef std::pair<uint32, base::TimeTicks> RtcpSendTimePair;
typedef std::map<uint32, base::TimeTicks> RtcpSendTimeMap;
typedef std::queue<RtcpSendTimePair> RtcpSendTimeQueue;

class RtpReceiverStatistics {
 public:
  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,  // 24 bits valid.
                             uint32* extended_high_sequence_number,
                             uint32* jitter) = 0;

  virtual ~RtpReceiverStatistics() {}
};

// TODO(hclam): This should be renamed to RtcpSession.
class Rtcp {
 public:
  Rtcp(const RtcpCastMessageCallback& cast_callback,
       const RtcpRttCallback& rtt_callback,
       const RtcpLogMessageCallback& log_callback,
       base::TickClock* clock,  // Not owned.
       PacedPacketSender* packet_sender,  // Not owned.
       uint32 local_ssrc,
       uint32 remote_ssrc);

  virtual ~Rtcp();

  // Send a RTCP sender report.
  // |current_time| is the current time reported by a tick clock.
  // |current_time_as_rtp_timestamp| is the corresponding RTP timestamp.
  // |send_packet_count| is the number of packets sent.
  // |send_octet_count| is the number of octets sent.
  void SendRtcpFromRtpSender(
      base::TimeTicks current_time,
      uint32 current_time_as_rtp_timestamp,
      uint32 send_packet_count,
      size_t send_octet_count);

  // |cast_message| and |rtcp_events| is optional; if |cast_message| is
  // provided the RTCP receiver report will append a Cast message containing
  // Acks and Nacks; |target_delay| is sent together with |cast_message|.
  // If |rtcp_events| is provided the RTCP receiver report will append the
  // log messages.
  void SendRtcpFromRtpReceiver(
      const RtcpCastMessage* cast_message,
      base::TimeDelta target_delay,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
      RtpReceiverStatistics* rtp_receiver_statistics);

  // Submit a received packet to this object. The packet will be parsed
  // and used to maintain a RTCP session.
  // Returns false if this is not a RTCP packet or it is not directed to
  // this session, e.g. SSRC doesn't match.
  bool IncomingRtcpPacket(const uint8* data, size_t length);

  // TODO(miu): Clean up this method and downstream code: Only VideoSender uses
  // this (for congestion control), and only the |rtt| and |avg_rtt| values, and
  // it's not clear that any of the downstream code is doing the right thing
  // with this data.
  bool Rtt(base::TimeDelta* rtt,
           base::TimeDelta* avg_rtt,
           base::TimeDelta* min_rtt,
           base::TimeDelta* max_rtt) const;

  // If available, returns true and sets the output arguments to the latest
  // lip-sync timestamps gleaned from the sender reports.  While the sender
  // provides reference NTP times relative to its own wall clock, the
  // |reference_time| returned here has been translated to the local
  // CastEnvironment clock.
  bool GetLatestLipSyncTimes(uint32* rtp_timestamp,
                             base::TimeTicks* reference_time) const;

  void OnReceivedReceiverLog(const RtcpReceiverLogMessage& receiver_log);

 protected:
  void OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction);
  void OnReceivedLipSyncInfo(uint32 rtp_timestamp,
                             uint32 ntp_seconds,
                             uint32 ntp_fraction);

 private:
  class RtcpMessageHandlerImpl;

  void OnReceivedDelaySinceLastReport(uint32 last_report,
                                      uint32 delay_since_last_report);

  void OnReceivedCastFeedback(const RtcpCastMessage& cast_message);

  void UpdateRtt(const base::TimeDelta& sender_delay,
                 const base::TimeDelta& receiver_delay);

  void SaveLastSentNtpTime(const base::TimeTicks& now,
                           uint32 last_ntp_seconds,
                           uint32 last_ntp_fraction);

  const RtcpCastMessageCallback cast_callback_;
  const RtcpRttCallback rtt_callback_;
  const RtcpLogMessageCallback log_callback_;
  base::TickClock* const clock_;  // Not owned by this class.
  const scoped_ptr<RtcpSender> rtcp_sender_;
  const uint32 local_ssrc_;
  const uint32 remote_ssrc_;
  const scoped_ptr<RtcpMessageHandlerImpl> handler_;
  const scoped_ptr<RtcpReceiver> rtcp_receiver_;

  RtcpSendTimeMap last_reports_sent_map_;
  RtcpSendTimeQueue last_reports_sent_queue_;

  // The truncated (i.e., 64-->32-bit) NTP timestamp provided in the last report
  // from the remote peer, along with the local time at which the report was
  // received.  These values are used for ping-pong'ing NTP timestamps between
  // the peers so that they can estimate the network's round-trip time.
  uint32 last_report_truncated_ntp_;
  base::TimeTicks time_last_report_received_;

  // Maintains a smoothed offset between the local clock and the remote clock.
  // Calling this member's Current() method is only valid if
  // |time_last_report_received_| is not "null."
  ClockDriftSmoother local_clock_ahead_by_;

  // Latest "lip sync" info from the sender.  The sender provides the RTP
  // timestamp of some frame of its choosing and also a corresponding reference
  // NTP timestamp sampled from a clock common to all media streams.  It is
  // expected that the sender will update this data regularly and in a timely
  // manner (e.g., about once per second).
  uint32 lip_sync_rtp_timestamp_;
  uint64 lip_sync_ntp_timestamp_;

  base::TimeDelta rtt_;
  base::TimeDelta min_rtt_;
  base::TimeDelta max_rtt_;
  int number_of_rtt_in_avg_;
  base::TimeDelta avg_rtt_;

  DISALLOW_COPY_AND_ASSIGN(Rtcp);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_H_
