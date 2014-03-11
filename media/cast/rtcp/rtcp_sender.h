// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_SENDER_H_
#define MEDIA_CAST_RTCP_RTCP_SENDER_H_

#include <list>
#include <string>

#include "media/cast/cast_config.h"
#include "media/cast/cast_defines.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/rtcp/rtcp_builder.h"

namespace media {
namespace cast {

// We limit the size of receiver logs to avoid queuing up packets. We also
// do not need the amount of redundancy that results from filling up every
// RTCP packet with log messages. This number should give a redundancy of
// about 2-3 per log message.
const size_t kMaxReceiverLogBytes = 200;

class ReceiverRtcpEventSubscriber;

// TODO(mikhal): Resolve duplication between this and RtcpBuilder.
class RtcpSender {
 public:
  RtcpSender(scoped_refptr<CastEnvironment> cast_environment,
             transport::PacedPacketSender* outgoing_transport,
             uint32 sending_ssrc,
             const std::string& c_name);

  virtual ~RtcpSender();

  // Returns true if |event| is an interesting receiver event.
  // Such an event should be sent via RTCP.
  static bool IsReceiverEvent(const media::cast::CastLoggingEvent& event);

  void SendRtcpFromRtpReceiver(
      uint32 packet_type_flags,
      const transport::RtcpReportBlock* report_block,
      const RtcpReceiverReferenceTimeReport* rrtr,
      const RtcpCastMessage* cast_message,
      const ReceiverRtcpEventSubscriber* event_subscriber);
  enum RtcpPacketType {
    kRtcpSr = 0x0002,
    kRtcpRr = 0x0004,
    kRtcpBye = 0x0008,
    kRtcpPli = 0x0010,
    kRtcpNack = 0x0020,
    kRtcpFir = 0x0040,
    kRtcpSrReq = 0x0200,
    kRtcpDlrr = 0x0400,
    kRtcpRrtr = 0x0800,
    kRtcpRpsi = 0x8000,
    kRtcpRemb = 0x10000,
    kRtcpCast = 0x20000,
    kRtcpSenderLog = 0x40000,
    kRtcpReceiverLog = 0x80000,
  };

 private:
  void BuildRR(const transport::RtcpReportBlock* report_block,
               Packet* packet) const;

  void AddReportBlocks(const transport::RtcpReportBlock& report_block,
                       Packet* packet) const;

  void BuildSdec(Packet* packet) const;

  void BuildPli(uint32 remote_ssrc, Packet* packet) const;

  void BuildRemb(const RtcpRembMessage* remb, Packet* packet) const;

  void BuildRpsi(const RtcpRpsiMessage* rpsi, Packet* packet) const;

  void BuildNack(const RtcpNackMessage* nack, Packet* packet) const;

  void BuildBye(Packet* packet) const;

  void BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                 Packet* packet) const;

  void BuildCast(const RtcpCastMessage* cast_message, Packet* packet) const;

  void BuildReceiverLog(
      const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
      Packet* packet) const;

  inline void BitrateToRembExponentBitrate(uint32 bitrate,
                                           uint8* exponent,
                                           uint32* mantissa) const {
    // 6 bit exponent and a 18 bit mantissa.
    *exponent = 0;
    for (int i = 0; i < 64; ++i) {
      if (bitrate <= (262143u << i)) {
        *exponent = i;
        break;
      }
    }
    *mantissa = (bitrate >> *exponent);
  }

  const uint32 ssrc_;
  const std::string c_name_;

  // Not owned by this class.
  transport::PacedPacketSender* const transport_;
  scoped_refptr<CastEnvironment> cast_environment_;

  DISALLOW_COPY_AND_ASSIGN(RtcpSender);
};

}  // namespace cast
}  // namespace media
#endif  // MEDIA_CAST_RTCP_RTCP_SENDER_H_
