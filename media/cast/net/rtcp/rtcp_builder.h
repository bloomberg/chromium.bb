// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_NET_RTCP_SENDER_H_
#define MEDIA_CAST_NET_RTCP_NET_RTCP_SENDER_H_

#include <list>
#include <string>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

class RtcpBuilder {
 public:
  RtcpBuilder(PacedPacketSender* const paced_packet_sender,
              uint32 sending_ssrc,
              const std::string& c_name);

  virtual ~RtcpBuilder();

  void SendRtcpFromRtpSender(uint32 packet_type_flags,
                             const RtcpSenderInfo* sender_info,
                             const RtcpDlrrReportBlock* dlrr,
                             RtcpSenderLogMessage* sender_log);

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
  void BuildSR(const RtcpSenderInfo& sender_info,
               const RtcpReportBlock* report_block,
               std::vector<uint8>* packet) const;

  void AddReportBlocks(const RtcpReportBlock& report_block,
                       std::vector<uint8>* packet) const;

  void BuildSdec(std::vector<uint8>* packet) const;

  void BuildBye(std::vector<uint8>* packet) const;

  void BuildDlrrRb(const RtcpDlrrReportBlock* dlrr,
                   std::vector<uint8>* packet) const;
  void BuildSenderLog(RtcpSenderLogMessage* sender_log_message,
                      std::vector<uint8>* packet) const;

  const uint32 ssrc_;
  const std::string c_name_;

  // Not owned by this class.
  PacedPacketSender* transport_;

  DISALLOW_COPY_AND_ASSIGN(RtcpBuilder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_NET_RTCP_SENDER_H_
