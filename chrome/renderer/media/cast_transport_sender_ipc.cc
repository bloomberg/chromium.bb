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

CastTransportSenderIPC::ClientCallbacks::ClientCallbacks() {}
CastTransportSenderIPC::ClientCallbacks::~ClientCallbacks() {}

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

void CastTransportSenderIPC::InitializeAudio(
    const media::cast::CastTransportRtpConfig& config,
    const media::cast::RtcpCastMessageCallback& cast_message_cb,
    const media::cast::RtcpRttCallback& rtt_cb) {
  clients_[config.ssrc].cast_message_cb = cast_message_cb;
  clients_[config.ssrc].rtt_cb = rtt_cb;
  Send(new CastHostMsg_InitializeAudio(channel_id_, config));
}

void CastTransportSenderIPC::InitializeVideo(
    const media::cast::CastTransportRtpConfig& config,
    const media::cast::RtcpCastMessageCallback& cast_message_cb,
    const media::cast::RtcpRttCallback& rtt_cb) {
  clients_[config.ssrc].cast_message_cb = cast_message_cb;
  clients_[config.ssrc].rtt_cb = rtt_cb;
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

void CastTransportSenderIPC::SendSenderReport(
    uint32 ssrc,
    base::TimeTicks current_time,
    uint32 current_time_as_rtp_timestamp) {
  Send(new CastHostMsg_SendSenderReport(channel_id_,
                                        ssrc,
                                        current_time,
                                        current_time_as_rtp_timestamp));
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

void CastTransportSenderIPC::OnNotifyStatusChange(
    media::cast::CastTransportStatus status) {
  status_callback_.Run(status);
}

void CastTransportSenderIPC::OnRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  raw_events_callback_.Run(packet_events, frame_events);
}

void CastTransportSenderIPC::OnRtt(
    uint32 ssrc,
    const media::cast::RtcpRttReport& rtt_report) {
  ClientMap::iterator it = clients_.find(ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received RTT report from for unknown SSRC: " << ssrc;
    return;
  }
  if (it->second.rtt_cb.is_null())
    return;
  it->second.rtt_cb.Run(
      rtt_report.rtt,
      rtt_report.avg_rtt,
      rtt_report.min_rtt,
      rtt_report.max_rtt);
}

void CastTransportSenderIPC::OnRtcpCastMessage(
    uint32 ssrc,
    const media::cast::RtcpCastMessage& cast_message) {
  ClientMap::iterator it = clients_.find(ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received cast message from for unknown SSRC: " << ssrc;
    return;
  }
  if (it->second.cast_message_cb.is_null())
    return;
  it->second.cast_message_cb.Run(cast_message);
}

void CastTransportSenderIPC::Send(IPC::Message* message) {
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->Send(message);
  } else {
    delete message;
  }
}
