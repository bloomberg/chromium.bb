// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
#define CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_

#include "base/message_loop/message_loop_proxy.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"

// This implementation of the CastTransportSender interface
// communicates with the browser process over IPC and relays
// all calls to/from the transport sender to the browser process.
// The primary reason for this arrangement is to give the
// renderer less direct control over the UDP sockets.
class CastTransportSenderIPC
    : public media::cast::CastTransportSender {
 public:
  CastTransportSenderIPC(
      const net::IPEndPoint& remote_end_point,
      const media::cast::CastTransportStatusCallback& status_cb,
      const media::cast::BulkRawEventsCallback& raw_events_cb);

  virtual ~CastTransportSenderIPC();

  // media::cast::CastTransportSender implementation.
  virtual void SetPacketReceiver(
      const media::cast::PacketReceiverCallback& packet_callback)
      OVERRIDE;
  virtual void InitializeAudio(
      const media::cast::CastTransportRtpConfig& config) OVERRIDE;
  virtual void InitializeVideo(
      const media::cast::CastTransportRtpConfig& config) OVERRIDE;
  virtual void InsertCodedAudioFrame(
      const media::cast::EncodedFrame& audio_frame) OVERRIDE;
  virtual void InsertCodedVideoFrame(
      const media::cast::EncodedFrame& video_frame) OVERRIDE;
  virtual void SendRtcpFromRtpSender(
      uint32 packet_type_flags,
      uint32 ntp_seconds,
      uint32 ntp_fraction,
      uint32 rtp_timestamp,
      const media::cast::RtcpDlrrReportBlock& dlrr,
      uint32 sending_ssrc,
      const std::string& c_name) OVERRIDE;
  virtual void ResendPackets(
      bool is_audio,
      const media::cast::MissingFramesAndPacketsMap& missing_packets,
      bool cancel_rtx_if_not_in_list,
      base::TimeDelta dedupe_window)
      OVERRIDE;

  void OnReceivedPacket(const media::cast::Packet& packet);
  void OnNotifyStatusChange(
      media::cast::CastTransportStatus status);
  void OnRawEvents(const std::vector<media::cast::PacketEvent>& packet_events);

 private:
  void Send(IPC::Message* message);

  int32 channel_id_;
  media::cast::PacketReceiverCallback packet_callback_;
  media::cast::CastTransportStatusCallback status_callback_;
  media::cast::BulkRawEventsCallback raw_events_callback_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderIPC);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
