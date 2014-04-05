// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_H_
#define MEDIA_CAST_RTCP_RTCP_H_

#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {

class LocalRtcpReceiverFeedback;
class LocalRtcpRttFeedback;
class PacedPacketSender;
class RtcpReceiver;
class RtcpSender;

typedef std::pair<uint32, base::TimeTicks> RtcpSendTimePair;
typedef std::map<uint32, base::TimeTicks> RtcpSendTimeMap;
typedef std::queue<RtcpSendTimePair> RtcpSendTimeQueue;

class RtcpSenderFeedback {
 public:
  virtual void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) = 0;

  virtual ~RtcpSenderFeedback() {}
};

class RtpReceiverStatistics {
 public:
  virtual void GetStatistics(uint8* fraction_lost,
                             uint32* cumulative_lost,  // 24 bits valid.
                             uint32* extended_high_sequence_number,
                             uint32* jitter) = 0;

  virtual ~RtpReceiverStatistics() {}
};

class Rtcp {
 public:
  // Rtcp accepts two transports, one to be used by Cast senders
  // (CastTransportSender) only, and the other (PacedPacketSender) should only
  // be used by the Cast receivers and test applications.
  Rtcp(scoped_refptr<CastEnvironment> cast_environment,
       RtcpSenderFeedback* sender_feedback,
       transport::CastTransportSender* const transport_sender,  // Send-side.
       transport::PacedPacketSender* paced_packet_sender,       // Receive side.
       RtpReceiverStatistics* rtp_receiver_statistics,
       RtcpMode rtcp_mode,
       const base::TimeDelta& rtcp_interval,
       uint32 local_ssrc,
       uint32 remote_ssrc,
       const std::string& c_name);

  virtual ~Rtcp();

  static bool IsRtcpPacket(const uint8* rtcp_buffer, size_t length);

  static uint32 GetSsrcOfSender(const uint8* rtcp_buffer, size_t length);

  base::TimeTicks TimeToSendNextRtcpReport();
  // |sender_log_message| is optional; without it no log messages will be
  // attached to the RTCP report; instead a normal RTCP send report will be
  // sent.
  // Additionally if all messages in |sender_log_message| does
  // not fit in the packet the |sender_log_message| will contain the remaining
  // unsent messages.
  void SendRtcpFromRtpSender(
      const transport::RtcpSenderLogMessage& sender_log_message,
      transport::RtcpSenderInfo sender_info);

  // |cast_message| and |rtcp_events| is optional; if |cast_message| is
  // provided the RTCP receiver report will append a Cast message containing
  // Acks and Nacks; if |rtcp_events| is provided the RTCP receiver report
  // will append the log messages.
  void SendRtcpFromRtpReceiver(
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events);

  void IncomingRtcpPacket(const uint8* rtcp_buffer, size_t length);
  bool Rtt(base::TimeDelta* rtt,
           base::TimeDelta* avg_rtt,
           base::TimeDelta* min_rtt,
           base::TimeDelta* max_rtt) const;
  bool RtpTimestampInSenderTime(int frequency,
                                uint32 rtp_timestamp,
                                base::TimeTicks* rtp_timestamp_in_ticks) const;

  // Set the history size to record Cast receiver events. The event history is
  // used to remove duplicates. The history will store at most |size| events.
  void SetCastReceiverEventHistorySize(size_t size);

  // Update the target delay. Will be added to every sender report.
  void SetTargetDelay(base::TimeDelta target_delay);

 protected:
  int CheckForWrapAround(uint32 new_timestamp, uint32 old_timestamp) const;

  void OnReceivedLipSyncInfo(uint32 rtp_timestamp,
                             uint32 ntp_seconds,
                             uint32 ntp_fraction);

 private:
  friend class LocalRtcpRttFeedback;
  friend class LocalRtcpReceiverFeedback;

  void SendRtcp(const base::TimeTicks& now,
                uint32 packet_type_flags,
                uint32 media_ssrc,
                const RtcpCastMessage* cast_message);

  void OnReceivedNtp(uint32 ntp_seconds, uint32 ntp_fraction);

  void OnReceivedDelaySinceLastReport(uint32 receivers_ssrc,
                                      uint32 last_report,
                                      uint32 delay_since_last_report);

  void OnReceivedSendReportRequest();

  void UpdateRtt(const base::TimeDelta& sender_delay,
                 const base::TimeDelta& receiver_delay);

  void UpdateNextTimeToSendRtcp();

  void SaveLastSentNtpTime(const base::TimeTicks& now,
                           uint32 last_ntp_seconds,
                           uint32 last_ntp_fraction);

  scoped_refptr<CastEnvironment> cast_environment_;
  transport::CastTransportSender* const transport_sender_;
  const base::TimeDelta rtcp_interval_;
  const RtcpMode rtcp_mode_;
  const uint32 local_ssrc_;
  const uint32 remote_ssrc_;
  const std::string c_name_;

  // Not owned by this class.
  RtpReceiverStatistics* const rtp_receiver_statistics_;

  scoped_ptr<LocalRtcpRttFeedback> rtt_feedback_;
  scoped_ptr<LocalRtcpReceiverFeedback> receiver_feedback_;
  scoped_ptr<RtcpSender> rtcp_sender_;
  scoped_ptr<RtcpReceiver> rtcp_receiver_;

  base::TimeTicks next_time_to_send_rtcp_;
  RtcpSendTimeMap last_reports_sent_map_;
  RtcpSendTimeQueue last_reports_sent_queue_;
  base::TimeTicks time_last_report_received_;
  uint32 last_report_received_;

  uint32 last_received_rtp_timestamp_;
  uint32 last_received_ntp_seconds_;
  uint32 last_received_ntp_fraction_;

  base::TimeDelta rtt_;
  base::TimeDelta min_rtt_;
  base::TimeDelta max_rtt_;
  int number_of_rtt_in_avg_;
  float avg_rtt_ms_;
  uint16 target_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(Rtcp);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_H_
