// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_transport_host_filter.h"
#include "media/cast/transport/cast_transport_sender.h"

namespace cast {

CastTransportHostFilter::CastTransportHostFilter() {
}

CastTransportHostFilter::~CastTransportHostFilter() {
}

bool CastTransportHostFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(CastTransportHostFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(CastHostMsg_New, OnNew)
    IPC_MESSAGE_HANDLER(CastHostMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertCodedAudioFrame,
                        OnInsertCodedAudioFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertCodedVideoFrame,
                        OnInsertCodedVideoFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendRtcpFromRtpSender,
                        OnSendRtcpFromRtpSender);
    IPC_MESSAGE_HANDLER(CastHostMsg_ResendPackets,
                        OnResendPackets)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP_EX();
  return handled;
}

void CastTransportHostFilter::NotifyStatusChange(
    int32 channel_id,
    media::cast::transport::CastTransportStatus status) {
  Send(new CastMsg_NotifyStatusChange(channel_id, status));
}

void CastTransportHostFilter::ReceivedPacket(
    int32 channel_id,
    scoped_ptr<media::cast::transport::Packet> packet) {
  Send(new CastMsg_ReceivedPacket(channel_id, *packet));
}

void CastTransportHostFilter::ReceivedRtpStatistics(
    int32 channel_id,
    bool audio,
    const media::cast::transport::RtcpSenderInfo& sender_info,
    base::TimeTicks time_sent,
    uint32 rtp_timestamp) {
  Send(new CastMsg_RtpStatistics(channel_id,
                                 audio,
                                 sender_info,
                                 time_sent,
                                 rtp_timestamp));
}

void CastTransportHostFilter::OnNew(
    int32 channel_id,
    const media::cast::transport::CastTransportConfig& config) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    id_map_.Remove(channel_id);
  }

  sender = media::cast::transport::CastTransportSender::
      CreateCastTransportSender(
          &clock_,
          config,
          base::Bind(&CastTransportHostFilter::NotifyStatusChange,
                     base::Unretained(this),
                     channel_id),
          base::MessageLoopProxy::current());
  sender->SetPacketReceiver(
          base::Bind(&CastTransportHostFilter::ReceivedPacket,
                     base::Unretained(this),
                     channel_id));
  sender->SubscribeAudioRtpStatsCallback(
          base::Bind(&CastTransportHostFilter::ReceivedRtpStatistics,
                     base::Unretained(this),
                     channel_id,
                     true  /* audio */));
  sender->SubscribeVideoRtpStatsCallback(
          base::Bind(&CastTransportHostFilter::ReceivedRtpStatistics,
                     base::Unretained(this),
                     channel_id,
                     false /* not audio */));

  id_map_.AddWithID(sender, channel_id);
}

void CastTransportHostFilter::OnDelete(int32 channel_id) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    id_map_.Remove(channel_id);
  } else {
    DVLOG(1) << "CastTransportHostFilter::Delete called "
             << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnInsertCodedAudioFrame(
    int32 channel_id,
    const media::cast::transport::EncodedAudioFrame& audio_frame,
    base::TimeTicks recorded_time) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InsertCodedAudioFrame(&audio_frame, recorded_time);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertCodedAudioFrame "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnInsertCodedVideoFrame(
    int32 channel_id,
    const media::cast::transport::EncodedVideoFrame& video_frame,
    base::TimeTicks capture_time) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->InsertCodedVideoFrame(&video_frame, capture_time);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertCodedVideoFrame "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendRtcpFromRtpSender(
    int32 channel_id,
    const media::cast::transport::SendRtcpFromRtpSenderData& data,
    const media::cast::transport::RtcpSenderInfo& sender_info,
    const media::cast::transport::RtcpDlrrReportBlock& dlrr,
    const media::cast::transport::RtcpSenderLogMessage& sender_log) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->SendRtcpFromRtpSender(data.packet_type_flags,
                                  sender_info,
                                  dlrr,
                                  sender_log,
                                  data.sending_ssrc,
                                  data.c_name);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendRtcpFromRtpSender "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnResendPackets(
    int32 channel_id,
    bool is_audio,
    const media::cast::MissingFramesAndPacketsMap& missing_packets) {
  media::cast::transport::CastTransportSender* sender =
      id_map_.Lookup(channel_id);
  if (sender) {
    sender->ResendPackets(is_audio, missing_packets);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnResendPackets on non-existing channel";
  }
}

}  // namespace cast
