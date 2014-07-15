// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the Cast transport API.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "net/base/ip_endpoint.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_START CastMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(
    media::cast::EncodedFrame::Dependency,
    media::cast::EncodedFrame::DEPENDENCY_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::Codec,
                          media::cast::CODEC_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::CastTransportStatus,
                          media::cast::CAST_TRANSPORT_STATUS_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::CastLoggingEvent,
                          media::cast::kNumOfLoggingEvents)
IPC_ENUM_TRAITS_MAX_VALUE(media::cast::EventMediaType,
                          media::cast::EVENT_MEDIA_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(media::cast::EncodedFrame)
  IPC_STRUCT_TRAITS_MEMBER(dependency)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(referenced_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(reference_time)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::RtcpDlrrReportBlock)
  IPC_STRUCT_TRAITS_MEMBER(last_rr)
  IPC_STRUCT_TRAITS_MEMBER(delay_since_last_rr)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::CastTransportRtpConfig)
  IPC_STRUCT_TRAITS_MEMBER(ssrc)
  IPC_STRUCT_TRAITS_MEMBER(rtp_payload_type)
  IPC_STRUCT_TRAITS_MEMBER(stored_frames)
  IPC_STRUCT_TRAITS_MEMBER(aes_key)
  IPC_STRUCT_TRAITS_MEMBER(aes_iv_mask)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::SendRtcpFromRtpSenderData)
  IPC_STRUCT_TRAITS_MEMBER(packet_type_flags)
  IPC_STRUCT_TRAITS_MEMBER(sending_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(c_name)
  IPC_STRUCT_TRAITS_MEMBER(ntp_seconds)
  IPC_STRUCT_TRAITS_MEMBER(ntp_fraction)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::PacketEvent)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(max_packet_id)
  IPC_STRUCT_TRAITS_MEMBER(packet_id)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
IPC_STRUCT_TRAITS_END()

// Cast messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL2(CastMsg_ReceivedPacket,
                     int32 /* channel_id */,
                     media::cast::Packet /* packet */)

IPC_MESSAGE_CONTROL2(
    CastMsg_NotifyStatusChange,
    int32 /* channel_id */,
    media::cast::CastTransportStatus /* status */)

IPC_MESSAGE_CONTROL2(CastMsg_RawEvents,
                     int32 /* channel_id */,
                     std::vector<media::cast::PacketEvent> /* packet_events */)

// Cast messages sent from the renderer to the browser.

IPC_MESSAGE_CONTROL2(
  CastHostMsg_InitializeAudio,
  int32 /*channel_id*/,
  media::cast::CastTransportRtpConfig /*config*/)

IPC_MESSAGE_CONTROL2(
  CastHostMsg_InitializeVideo,
  int32 /*channel_id*/,
  media::cast::CastTransportRtpConfig /*config*/)

IPC_MESSAGE_CONTROL2(
    CastHostMsg_InsertCodedAudioFrame,
    int32 /* channel_id */,
    media::cast::EncodedFrame /* audio_frame */)

IPC_MESSAGE_CONTROL2(
    CastHostMsg_InsertCodedVideoFrame,
    int32 /* channel_id */,
    media::cast::EncodedFrame /* video_frame */)

IPC_MESSAGE_CONTROL3(
    CastHostMsg_SendRtcpFromRtpSender,
    int32 /* channel_id */,
    media::cast::SendRtcpFromRtpSenderData /* data */,
    media::cast::RtcpDlrrReportBlock /* dlrr */)

IPC_MESSAGE_CONTROL5(
    CastHostMsg_ResendPackets,
    int32 /* channel_id */,
    bool /* is_audio */,
    media::cast::MissingFramesAndPacketsMap /* missing_packets */,
    bool /* cancel_rtx_if_not_in_list */,
    base::TimeDelta /* dedupe_window */)

IPC_MESSAGE_CONTROL2(
    CastHostMsg_New,
    int32 /* channel_id */,
    net::IPEndPoint /*remote_end_point*/)

IPC_MESSAGE_CONTROL1(
    CastHostMsg_Delete,
    int32 /* channel_id */)
