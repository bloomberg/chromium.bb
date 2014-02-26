// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the Cast transport API.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "media/cast/cast_sender.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_START CastMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::cast::transport::AudioCodec,
                          media::cast::transport::kAudioCodecLast)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::transport::VideoCodec,
                          media::cast::transport::kVideoCodecLast)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::transport::RtcpSenderFrameStatus,
                          media::cast::transport::kRtcpSenderFrameStatusLast)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::transport::CastTransportStatus,
                          media::cast::transport::CAST_TRANSPORT_STATUS_LAST)

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::EncodedAudioFrame)
  IPC_STRUCT_TRAITS_MEMBER(codec)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::EncodedVideoFrame)
  IPC_STRUCT_TRAITS_MEMBER(codec)
  IPC_STRUCT_TRAITS_MEMBER(key_frame)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(last_referenced_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::RtcpSenderInfo)
  IPC_STRUCT_TRAITS_MEMBER(ntp_seconds)
  IPC_STRUCT_TRAITS_MEMBER(ntp_fraction)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(send_packet_count)
  IPC_STRUCT_TRAITS_MEMBER(send_octet_count)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::RtcpDlrrReportBlock)
  IPC_STRUCT_TRAITS_MEMBER(last_rr)
  IPC_STRUCT_TRAITS_MEMBER(delay_since_last_rr)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::RtcpSenderFrameLogMessage)
  IPC_STRUCT_TRAITS_MEMBER(frame_status)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::RtpConfig)
  IPC_STRUCT_TRAITS_MEMBER(history_ms)
  IPC_STRUCT_TRAITS_MEMBER(max_delay_ms)
  IPC_STRUCT_TRAITS_MEMBER(payload_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::CastTransportConfig)
  IPC_STRUCT_TRAITS_MEMBER(receiver_endpoint)
  IPC_STRUCT_TRAITS_MEMBER(local_endpoint)
  IPC_STRUCT_TRAITS_MEMBER(audio_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(video_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(video_codec)
  IPC_STRUCT_TRAITS_MEMBER(audio_codec)
  IPC_STRUCT_TRAITS_MEMBER(audio_frequency)
  IPC_STRUCT_TRAITS_MEMBER(audio_channels)
  IPC_STRUCT_TRAITS_MEMBER(audio_rtp_config)
  IPC_STRUCT_TRAITS_MEMBER(video_rtp_config)
  IPC_STRUCT_TRAITS_MEMBER(aes_key)
  IPC_STRUCT_TRAITS_MEMBER(aes_iv_mask)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::transport::SendRtcpFromRtpSenderData)
  IPC_STRUCT_TRAITS_MEMBER(packet_type_flags)
  IPC_STRUCT_TRAITS_MEMBER(sending_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(c_name)
IPC_STRUCT_TRAITS_END()


// Cast messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL2(CastMsg_ReceivedPacket,
                     int32 /* channel_id */,
                     media::cast::Packet /* packet */);

IPC_MESSAGE_CONTROL2(
    CastMsg_NotifyStatusChange,
    int32 /* channel_id */,
    media::cast::transport::CastTransportStatus /* status */);

IPC_MESSAGE_CONTROL5(
    CastMsg_RtpStatistics,
    int32 /* channel_id */,
    bool /* audio */,
    media::cast::transport::RtcpSenderInfo /* sender_info */,
    base::TimeTicks /* time_sent */,
    uint32 /* rtp_timestamp */);


// Cast messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL3(
    CastHostMsg_InsertCodedAudioFrame,
    int32 /* channel_id */,
    media::cast::transport::EncodedAudioFrame /* audio_frame */,
    base::TimeTicks /* recorded_time */)

IPC_MESSAGE_CONTROL3(
    CastHostMsg_InsertCodedVideoFrame,
    int32 /* channel_id */,
    media::cast::transport::EncodedVideoFrame /* video_frame */,
    base::TimeTicks /* recorded_time */)

IPC_MESSAGE_CONTROL5(
    CastHostMsg_SendRtcpFromRtpSender,
    int32 /* channel_id */,
    media::cast::transport::SendRtcpFromRtpSenderData /* flags, ssrc, name */,
    media::cast::transport::RtcpSenderInfo /* sender_info */,
    media::cast::transport::RtcpDlrrReportBlock /* dlrr */,
    media::cast::transport::RtcpSenderLogMessage /* sender_log */)

IPC_MESSAGE_CONTROL3(
    CastHostMsg_ResendPackets,
    int32 /* channel_id */,
    bool /* is_audio */,
    media::cast::MissingFramesAndPacketsMap /* missing_packets */)

IPC_MESSAGE_CONTROL2(
    CastHostMsg_New,
    int32 /* channel_id */,
    media::cast::transport::CastTransportConfig /* config */);

IPC_MESSAGE_CONTROL1(
    CastHostMsg_Delete,
    int32 /* channel_id */);
