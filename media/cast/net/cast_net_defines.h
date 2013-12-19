// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_CAST_NET_DEFINES_H_
#define MEDIA_CAST_NET_CAST_NET_DEFINES_H_

#include <list>
#include "base/basictypes.h"

namespace media {
namespace cast {

// Rtcp defines.

// Log messages form sender to receiver.
enum RtcpSenderFrameStatus {
  kRtcpSenderFrameStatusUnknown = 0,
  kRtcpSenderFrameStatusDroppedByEncoder = 1,
  kRtcpSenderFrameStatusDroppedByFlowControl = 2,
  kRtcpSenderFrameStatusSentToNetwork = 3,
};

struct RtcpSenderFrameLogMessage {
  RtcpSenderFrameStatus frame_status;
  uint32 rtp_timestamp;
};

struct RtcpSenderInfo {
  // First three members are used for lipsync.
  // First two members are used for rtt.
  uint32 ntp_seconds;
  uint32 ntp_fraction;
  uint32 rtp_timestamp;
  uint32 send_packet_count;
  size_t send_octet_count;
};

struct RtcpReportBlock {
  uint32 remote_ssrc;  // SSRC of sender of this report.
  uint32 media_ssrc;  // SSRC of the RTP packet sender.
  uint8 fraction_lost;
  uint32 cumulative_lost;  // 24 bits valid.
  uint32 extended_high_sequence_number;
  uint32 jitter;
  uint32 last_sr;
  uint32 delay_since_last_sr;
};

struct RtcpDlrrReportBlock {
  uint32 last_rr;
  uint32 delay_since_last_rr;
};

typedef std::list<RtcpSenderFrameLogMessage> RtcpSenderLogMessage;

class FrameIdWrapHelper {
 public:
  FrameIdWrapHelper()
      : first_(true),
        frame_id_wrap_count_(0),
        range_(kLowRange) {}

  uint32 MapTo32bitsFrameId(const uint8 over_the_wire_frame_id) {
    if (first_) {
      first_ = false;
      if (over_the_wire_frame_id == 0xff) {
        // Special case for startup.
        return kStartFrameId;
      }
    }

    uint32 wrap_count = frame_id_wrap_count_;
    switch (range_) {
      case kLowRange:
        if (over_the_wire_frame_id > kLowRangeThreshold &&
            over_the_wire_frame_id < kHighRangeThreshold) {
          range_ = kMiddleRange;
        }
        if (over_the_wire_frame_id > kHighRangeThreshold) {
          // Wrap count was incremented in High->Low transition, but this frame
          // is 'old', actually from before the wrap count got incremented.
          --wrap_count;
        }
        break;
      case kMiddleRange:
        if (over_the_wire_frame_id > kHighRangeThreshold) {
          range_ = kHighRange;
        }
        break;
      case kHighRange:
        if (over_the_wire_frame_id < kLowRangeThreshold) {
          // Wrap-around detected.
          range_ = kLowRange;
          ++frame_id_wrap_count_;
          // Frame triggering wrap-around so wrap count should be incremented as
          // as well to match |frame_id_wrap_count_|.
          ++wrap_count;
        }
        break;
    }
    return (wrap_count << 8) + over_the_wire_frame_id;
  }

 private:
  enum Range {
    kLowRange,
    kMiddleRange,
    kHighRange,
  };

  static const uint8 kLowRangeThreshold = 0x0f;
  static const uint8 kHighRangeThreshold = 0xf0;
  static const uint32 kStartFrameId = GG_UINT32_C(0xffffffff);

  bool first_;
  uint32 frame_id_wrap_count_;
  Range range_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_NET_DEFINES_H_
