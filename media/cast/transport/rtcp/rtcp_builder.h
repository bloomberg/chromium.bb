// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_
#define MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_

#include <list>
#include <string>
#include <vector>

#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

class RtcpBuilder {
 public:
  explicit RtcpBuilder(PacedSender* const paced_packet_sender);

  virtual ~RtcpBuilder();

  void SendRtcpFromRtpSender(uint32 packet_type_flags,
                             const RtcpSenderInfo& sender_info,
                             const RtcpDlrrReportBlock& dlrr,
                             const RtcpSenderLogMessage& sender_log,
                             uint32 ssrc,
                             const std::string& c_name);

  enum RtcpPacketType {
    kRtcpSr     = 0x0002,
    kRtcpRr     = 0x0004,
    kRtcpBye    = 0x0008,
    kRtcpPli    = 0x0010,
    kRtcpNack   = 0x0020,
    kRtcpFir    = 0x0040,
    kRtcpSrReq  = 0x0200,
    kRtcpDlrr   = 0x0400,
    kRtcpRrtr   = 0x0800,
    kRtcpRpsi   = 0x8000,
    kRtcpRemb   = 0x10000,
    kRtcpCast   = 0x20000,
    kRtcpSenderLog = 0x40000,
    kRtcpReceiverLog = 0x80000,
  };

 private:
  bool BuildSR(const RtcpSenderInfo& sender_info, Packet* packet) const;
  bool BuildSdec(Packet* packet) const;
  bool BuildBye(Packet* packet) const;
  bool BuildDlrrRb(const RtcpDlrrReportBlock& dlrr,
                   Packet* packet) const;
  bool BuildSenderLog(const RtcpSenderLogMessage& sender_log_message,
                      Packet* packet) const;

  PacedSender* const transport_;  // Not owned by this class.
  uint32 ssrc_;
  std::string c_name_;

  DISALLOW_COPY_AND_ASSIGN(RtcpBuilder);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTCP_RTCP_BUILDER_H_
