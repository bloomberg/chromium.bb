// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_transport_sender_ipc.h"

#include "base/callback.h"
#include "base/id_map.h"
#include "chrome/common/cast_messages.h"
#include "chrome/renderer/media/cast_ipc_dispatcher.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/cast_sender.h"
#include "media/cast/transport/cast_transport_sender.h"

CastTransportSenderIPC::CastTransportSenderIPC(
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const media::cast::transport::CastTransportStatusCallback& status_cb)
    : status_callback_(status_cb) {
  if (CastIPCDispatcher::Get()) {
    channel_id_ = CastIPCDispatcher::Get()->AddSender(this);
  }
  Send(new CastHostMsg_New(channel_id_, local_end_point, remote_end_point));
}

CastTransportSenderIPC::~CastTransportSenderIPC() {
  Send(new CastHostMsg_Delete(channel_id_));
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->RemoveSender(channel_id_);
  }
}

void CastTransportSenderIPC::SetPacketReceiver(
    const media::cast::transport::PacketReceiverCallback& packet_callback) {
  packet_callback_ = packet_callback;
}

void CastTransportSenderIPC::InitializeAudio(
    const media::cast::transport::CastTransportAudioConfig& config) {
  Send(new CastHostMsg_InitializeAudio(channel_id_, config));
}

void CastTransportSenderIPC::InitializeVideo(
    const media::cast::transport::CastTransportVideoConfig& config) {
  Send(new CastHostMsg_InitializeVideo(channel_id_, config));
}

void CastTransportSenderIPC::InsertCodedAudioFrame(
    const media::cast::transport::EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  Send(new CastHostMsg_InsertCodedAudioFrame(channel_id_,
                                             *audio_frame,
                                             recorded_time));
}

void CastTransportSenderIPC::InsertCodedVideoFrame(
    const media::cast::transport::EncodedVideoFrame* video_frame,
    const base::TimeTicks& capture_time) {
  Send(new CastHostMsg_InsertCodedVideoFrame(channel_id_,
                                             *video_frame,
                                             capture_time));
}

void CastTransportSenderIPC::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const media::cast::transport::RtcpSenderInfo& sender_info,
    const media::cast::transport::RtcpDlrrReportBlock& dlrr,
    const media::cast::transport::RtcpSenderLogMessage& sender_log,
    uint32 sending_ssrc,
    const std::string& c_name) {
  struct media::cast::transport::SendRtcpFromRtpSenderData data;
  data.packet_type_flags = packet_type_flags;
  data.sending_ssrc = sending_ssrc;
  data.c_name = c_name;
  Send(new CastHostMsg_SendRtcpFromRtpSender(
      channel_id_,
      data,
      sender_info,
      dlrr,
      sender_log));
}

void CastTransportSenderIPC::ResendPackets(
    bool is_audio,
    const media::cast::MissingFramesAndPacketsMap& missing_packets) {
  Send(new CastHostMsg_ResendPackets(channel_id_,
                                     is_audio,
                                     missing_packets));
}

void CastTransportSenderIPC::SubscribeAudioRtpStatsCallback(
    const media::cast::transport::CastTransportRtpStatistics& callback) {
  audio_rtp_callback_ = callback;
}

void CastTransportSenderIPC::SubscribeVideoRtpStatsCallback(
    const media::cast::transport::CastTransportRtpStatistics& callback) {
  video_rtp_callback_ = callback;
}


void CastTransportSenderIPC::OnReceivedPacket(
    const media::cast::Packet& packet) {
  if (!packet_callback_.is_null()) {
    // TODO(hubbe): Perhaps an non-ownership-transferring cb here?
    scoped_ptr<media::cast::transport::Packet> packet_copy(
        new media::cast::transport::Packet(packet));
    packet_callback_.Run(packet_copy.Pass());
  } else {
    LOG(ERROR) << "CastIPCDispatcher::OnReceivedPacket "
               << "no packet callback yet.";
  }
}

void CastTransportSenderIPC::OnNotifyStatusChange(
    media::cast::transport::CastTransportStatus status) {
  status_callback_.Run(status);
}

void CastTransportSenderIPC::OnRtpStatistics(
    bool audio,
    const media::cast::transport::RtcpSenderInfo& sender_info,
    base::TimeTicks time_sent,
    uint32 rtp_timestamp) {
  const media::cast::transport::CastTransportRtpStatistics& callback =
      audio ? audio_rtp_callback_ : video_rtp_callback_;
  callback.Run(sender_info, time_sent, rtp_timestamp);
}

void CastTransportSenderIPC::Send(IPC::Message* message) {
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->Send(message);
  } else {
    delete message;
  }
}
