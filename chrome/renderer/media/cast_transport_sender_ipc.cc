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
    scoped_ptr<base::DictionaryValue> options,
    const media::cast::CastTransportStatusCallback& status_cb,
    const media::cast::BulkRawEventsCallback& raw_events_cb)
    : status_callback_(status_cb), raw_events_callback_(raw_events_cb) {
  if (CastIPCDispatcher::Get()) {
    channel_id_ = CastIPCDispatcher::Get()->AddSender(this);
  }
  Send(new CastHostMsg_New(channel_id_, remote_end_point, *options));
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

void CastTransportSenderIPC::InsertFrame(uint32 ssrc,
    const media::cast::EncodedFrame& frame) {
  Send(new CastHostMsg_InsertFrame(channel_id_, ssrc, frame));
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

void CastTransportSenderIPC::CancelSendingFrames(
    uint32 ssrc, const std::vector<uint32>& frame_ids) {
  Send(new CastHostMsg_CancelSendingFrames(channel_id_,
                                           ssrc,
                                           frame_ids));
}

void CastTransportSenderIPC::ResendFrameForKickstart(
    uint32 ssrc, uint32 frame_id) {
  Send(new CastHostMsg_ResendFrameForKickstart(channel_id_,
                                               ssrc,
                                               frame_id));
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

void CastTransportSenderIPC::OnRtt(uint32 ssrc, base::TimeDelta rtt) {
  ClientMap::iterator it = clients_.find(ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received RTT report from for unknown SSRC: " << ssrc;
    return;
  }
  if (!it->second.rtt_cb.is_null())
    it->second.rtt_cb.Run(rtt);
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
