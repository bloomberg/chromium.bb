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
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    scoped_ptr<base::DictionaryValue> options,
    const media::cast::PacketReceiverCallback& packet_callback,
    const media::cast::CastTransportStatusCallback& status_cb,
    const media::cast::BulkRawEventsCallback& raw_events_cb)
    : packet_callback_(packet_callback) ,
      status_callback_(status_cb),
      raw_events_callback_(raw_events_cb) {
  if (CastIPCDispatcher::Get()) {
    channel_id_ = CastIPCDispatcher::Get()->AddSender(this);
  }
  Send(new CastHostMsg_New(channel_id_,
                           local_end_point,
                           remote_end_point,
                           *options));
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

void CastTransportSenderIPC::AddValidSsrc(uint32 ssrc) {
  Send(new CastHostMsg_AddValidSsrc(channel_id_, ssrc));
}


void CastTransportSenderIPC::SendRtcpFromRtpReceiver(
    uint32 ssrc,
    uint32 sender_ssrc,
    const media::cast::RtcpTimeData& time_data,
    const media::cast::RtcpCastMessage* cast_message,
    base::TimeDelta target_delay,
    const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents*
    rtcp_events,
    const media::cast::RtpReceiverStatistics* rtp_receiver_statistics) {
  // To avoid copies, we put pointers to objects we don't really
  // own into scoped pointers and then very carefully extract them again
  // before the scoped pointers go out of scope.
  media::cast::SendRtcpFromRtpReceiver_Params params;
  params.ssrc = ssrc;
  params.sender_ssrc = sender_ssrc;
  params.time_data = time_data;
  if (cast_message) {
    params.cast_message.reset(
        const_cast<media::cast::RtcpCastMessage*>(cast_message));
  }
  params.target_delay = target_delay;
  if (rtcp_events) {
    params.rtcp_events.reset(
        const_cast<media::cast::ReceiverRtcpEventSubscriber::RtcpEvents*>(
            rtcp_events));
  }
  if (rtp_receiver_statistics) {
    params.rtp_receiver_statistics.reset(
        const_cast<media::cast::RtpReceiverStatistics*>(
            rtp_receiver_statistics));
  }
  // Note, params contains scoped_ptr<>, but this still works because
  // CastHostMsg_SendRtcpFromRtpReceiver doesn't take ownership, it
  // serializes it and remember the serialized form instead.
  Send(new CastHostMsg_SendRtcpFromRtpReceiver(channel_id_, params));

  ignore_result(params.rtp_receiver_statistics.release());
  ignore_result(params.cast_message.release());
  ignore_result(params.rtcp_events.release());
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

void CastTransportSenderIPC::Send(IPC::Message* message) {
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->Send(message);
  } else {
    delete message;
  }
}
