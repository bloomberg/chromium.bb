// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
#define CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_

#include "base/message_loop/message_loop_proxy.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/transport/cast_transport_sender.h"

// This implementation of the CastTransportSender interface
// communicates with the browser process over IPC and relays
// all calls to/from the transport sender to the browser process.
// The primary reason for this arrangement is to give the
// renderer less direct control over the UDP sockets.
class CastTransportSenderIPC
    : public media::cast::transport::CastTransportSender {
 public:
  CastTransportSenderIPC(
      const media::cast::transport::CastTransportConfig& config,
      const media::cast::transport::CastTransportStatusCallback& status_cb);

  virtual ~CastTransportSenderIPC();

  // media::cast::transport::CastTransportSender implementation.
  virtual void SetPacketReceiver(
      const media::cast::transport::PacketReceiverCallback& packet_callback)
      OVERRIDE;
  virtual void InsertCodedAudioFrame(
      const media::cast::transport::EncodedAudioFrame* audio_frame,
      const base::TimeTicks& recorded_time) OVERRIDE;
  virtual void InsertCodedVideoFrame(
      const media::cast::transport::EncodedVideoFrame* video_frame,
      const base::TimeTicks& capture_time) OVERRIDE;
  virtual void SendRtcpFromRtpSender(
      uint32 packet_type_flags,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      const media::cast::transport::RtcpDlrrReportBlock& dlrr,
      const media::cast::transport::RtcpSenderLogMessage& sender_log,
      uint32 sending_ssrc,
      const std::string& c_name) OVERRIDE;
  virtual void ResendPackets(
      bool is_audio,
      const media::cast::transport::MissingFramesAndPacketsMap& missing_packets)
      OVERRIDE;
  virtual void SubscribeAudioRtpStatsCallback(
      const media::cast::transport::CastTransportRtpStatistics& callback)
      OVERRIDE;
  virtual void SubscribeVideoRtpStatsCallback(
      const media::cast::transport::CastTransportRtpStatistics& callback)
      OVERRIDE;

  void OnReceivedPacket(const media::cast::transport::Packet& packet);
  void OnNotifyStatusChange(
      media::cast::transport::CastTransportStatus status);
  void OnRtpStatistics(
      bool audio,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      base::TimeTicks time_sent,
      uint32 rtp_timestamp);

 private:
  void Send(IPC::Message* message);

  int32 channel_id_;
  media::cast::transport::PacketReceiverCallback packet_callback_;
  media::cast::transport::CastTransportStatusCallback status_callback_;
  media::cast::transport::CastTransportRtpStatistics audio_rtp_callback_;
  media::cast::transport::CastTransportRtpStatistics video_rtp_callback_;
  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderIPC);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
