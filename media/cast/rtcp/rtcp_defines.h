// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_DEFINES_H_
#define MEDIA_CAST_RTCP_RTCP_DEFINES_H_

#include <list>
#include <map>
#include <set>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_net_defines.h"

namespace media {
namespace cast {

// Handle the per frame ACK and NACK messages.
class RtcpCastMessage {
 public:
  explicit RtcpCastMessage(uint32 media_ssrc);
  ~RtcpCastMessage();

  uint32 media_ssrc_;
  uint32 ack_frame_id_;
  MissingFramesAndPacketsMap missing_frames_and_packets_;
};

// Log messages from receiver to sender.
struct RtcpReceiverEventLogMessage {
  CastLoggingEvent type;
  base::TimeTicks event_timestamp;
  base::TimeDelta delay_delta;
  uint16 packet_id;
};

typedef std::list<RtcpReceiverEventLogMessage> RtcpReceiverEventLogMessages;

class RtcpReceiverFrameLogMessage {
 public:
  explicit RtcpReceiverFrameLogMessage(uint32 rtp_timestamp);
  ~RtcpReceiverFrameLogMessage();

  uint32 rtp_timestamp_;
  RtcpReceiverEventLogMessages event_log_messages_;
};

typedef std::list<RtcpReceiverFrameLogMessage> RtcpReceiverLogMessage;

struct RtcpRpsiMessage {
  uint32 remote_ssrc;
  uint8 payload_type;
  uint64 picture_id;
};

class RtcpNackMessage {
 public:
  RtcpNackMessage();
  ~RtcpNackMessage();

  uint32 remote_ssrc;
  std::list<uint16> nack_list;
};

class RtcpRembMessage {
 public:
  RtcpRembMessage();
  ~RtcpRembMessage();

  uint32 remb_bitrate;
  std::list<uint32> remb_ssrcs;
};

struct RtcpReceiverReferenceTimeReport {
  uint32 remote_ssrc;
  uint32 ntp_seconds;
  uint32 ntp_fraction;
};

inline bool operator==(RtcpSenderInfo lhs, RtcpSenderInfo rhs) {
  return lhs.ntp_seconds == rhs.ntp_seconds &&
      lhs.ntp_fraction == rhs.ntp_fraction &&
      lhs.rtp_timestamp == rhs.rtp_timestamp &&
      lhs.send_packet_count == rhs.send_packet_count &&
      lhs.send_octet_count == rhs.send_octet_count;
}

inline bool operator==(RtcpReceiverReferenceTimeReport lhs,
                       RtcpReceiverReferenceTimeReport rhs) {
  return lhs.remote_ssrc == rhs.remote_ssrc &&
      lhs.ntp_seconds == rhs.ntp_seconds &&
      lhs.ntp_fraction == rhs.ntp_fraction;
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_DEFINES_H_
