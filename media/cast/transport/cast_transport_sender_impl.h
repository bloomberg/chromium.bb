// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_IMPL_H_
#define MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "media/cast/transport/rtcp/rtcp_builder.h"
#include "media/cast/transport/transport_audio_sender.h"
#include "media/cast/transport/transport_video_sender.h"

namespace media {
namespace cast {
namespace transport {

class CastTransportSenderImpl : public CastTransportSender {
 public:
  // external_transport is only used for testing.
  // Note that SetPacketReceiver does not work if an external
  // transport is provided.
  CastTransportSenderImpl(
      net::NetLog* net_log,
      base::TickClock* clock,
      const CastTransportConfig& config,
      const CastTransportStatusCallback& status_callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
      PacketSender* external_transport);

  virtual ~CastTransportSenderImpl();

  // CastTransportSender implementation.
  virtual void SetPacketReceiver(const PacketReceiverCallback& packet_receiver)
      OVERRIDE;

  virtual void InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                     const base::TimeTicks& recorded_time)
      OVERRIDE;

  virtual void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                                     const base::TimeTicks& capture_time)
      OVERRIDE;

  virtual void SendRtcpFromRtpSender(uint32 packet_type_flags,
                                     const RtcpSenderInfo& sender_info,
                                     const RtcpDlrrReportBlock& dlrr,
                                     const RtcpSenderLogMessage& sender_log,
                                     uint32 sending_ssrc,
                                     const std::string& c_name) OVERRIDE;

  virtual void ResendPackets(bool is_audio,
                             const MissingFramesAndPacketsMap& missing_packets)
      OVERRIDE;

  virtual void SubscribeAudioRtpStatsCallback(
      const CastTransportRtpStatistics& callback) OVERRIDE;

  virtual void SubscribeVideoRtpStatsCallback(
      const CastTransportRtpStatistics& callback) OVERRIDE;

 private:
  scoped_ptr<UdpTransport> transport_;
  PacedSender pacer_;
  RtcpBuilder rtcp_builder_;
  TransportAudioSender audio_sender_;
  TransportVideoSender video_sender_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderImpl);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_IMPL_H_
