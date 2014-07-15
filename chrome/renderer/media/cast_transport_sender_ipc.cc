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

CastTransportSenderIPC::CastTransportSenderIPC(
    const net::IPEndPoint& remote_end_point,
    const media::cast::CastTransportStatusCallback& status_cb,
    const media::cast::BulkRawEventsCallback& raw_events_cb)
    : status_callback_(status_cb), raw_events_callback_(raw_events_cb) {
  if (CastIPCDispatcher::Get()) {
    channel_id_ = CastIPCDispatcher::Get()->AddSender(this);
  }
  Send(new CastHostMsg_New(channel_id_, remote_end_point));
}

CastTransportSenderIPC::~CastTransportSenderIPC() {
  Send(new CastHostMsg_Delete(channel_id_));
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->RemoveSender(channel_id_);
  }
}

void CastTransportSenderIPC::SetPacketReceiver(
    const media::cast::PacketReceiverCallback& packet_callback) {
  packet_callback_ = packet_callback;
}

void CastTransportSenderIPC::InitializeAudio(
    const media::cast::CastTransportRtpConfig& config) {
  Send(new CastHostMsg_InitializeAudio(channel_id_, config));
}

void CastTransportSenderIPC::InitializeVideo(
    const media::cast::CastTransportRtpConfig& config) {
  Send(new CastHostMsg_InitializeVideo(channel_id_, config));
}

void CastTransportSenderIPC::InsertCodedAudioFrame(
    const media::cast::EncodedFrame& audio_frame) {
  Send(new CastHostMsg_InsertCodedAudioFrame(channel_id_, audio_frame));
}

void CastTransportSenderIPC::InsertCodedVideoFrame(
    const media::cast::EncodedFrame& video_frame) {
  Send(new CastHostMsg_InsertCodedVideoFrame(channel_id_, video_frame));
}

void CastTransportSenderIPC::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    uint32 ntp_seconds,
    uint32 ntp_fraction,
    uint32 rtp_timestamp,
    const media::cast::RtcpDlrrReportBlock& dlrr,
    uint32 sending_ssrc,
    const std::string& c_name) {
  struct media::cast::SendRtcpFromRtpSenderData data;
  data.packet_type_flags = packet_type_flags;
  data.sending_ssrc = sending_ssrc;
  data.c_name = c_name;
  data.ntp_seconds = ntp_seconds;
  data.ntp_fraction = ntp_fraction;
  data.rtp_timestamp = rtp_timestamp;
  Send(new CastHostMsg_SendRtcpFromRtpSender(
      channel_id_,
      data,
      dlrr));
}

void CastTransportSenderIPC::ResendPackets(
    bool is_audio,
    const media::cast::MissingFramesAndPacketsMap& missing_packets,
    bool cancel_rtx_if_not_in_list,
    base::TimeDelta dedupe_window) {
  Send(new CastHostMsg_ResendPackets(channel_id_,
                                     is_audio,
                                     missing_packets,
                                     cancel_rtx_if_not_in_list,
                                     dedupe_window));
}

void CastTransportSenderIPC::OnReceivedPacket(
    const media::cast::Packet& packet) {
  if (!packet_callback_.is_null()) {
    // TODO(hubbe): Perhaps an non-ownership-transferring cb here?
    scoped_ptr<media::cast::Packet> packet_copy(
        new media::cast::Packet(packet));
    packet_callback_.Run(packet_copy.Pass());
  } else {
    DVLOG(1) << "CastIPCDispatcher::OnReceivedPacket no packet callback yet.";
  }
}

void CastTransportSenderIPC::OnNotifyStatusChange(
    media::cast::CastTransportStatus status) {
  status_callback_.Run(status);
}

void CastTransportSenderIPC::OnRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events) {
  raw_events_callback_.Run(packet_events);
}

void CastTransportSenderIPC::Send(IPC::Message* message) {
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->Send(message);
  } else {
    delete message;
  }
}
