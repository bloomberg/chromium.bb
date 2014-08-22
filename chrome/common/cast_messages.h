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
  IPC_STRUCT_TRAITS_MEMBER(new_playout_delay_ms)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::RtcpDlrrReportBlock)
  IPC_STRUCT_TRAITS_MEMBER(last_rr)
  IPC_STRUCT_TRAITS_MEMBER(delay_since_last_rr)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::CastTransportRtpConfig)
  IPC_STRUCT_TRAITS_MEMBER(ssrc)
  IPC_STRUCT_TRAITS_MEMBER(feedback_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(rtp_payload_type)
  IPC_STRUCT_TRAITS_MEMBER(stored_frames)
  IPC_STRUCT_TRAITS_MEMBER(aes_key)
  IPC_STRUCT_TRAITS_MEMBER(aes_iv_mask)
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

IPC_STRUCT_TRAITS_BEGIN(media::cast::FrameEvent)
  IPC_STRUCT_TRAITS_MEMBER(rtp_timestamp)
  IPC_STRUCT_TRAITS_MEMBER(frame_id)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(delay_delta)
  IPC_STRUCT_TRAITS_MEMBER(key_frame)
  IPC_STRUCT_TRAITS_MEMBER(target_bitrate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::RtcpCastMessage)
  IPC_STRUCT_TRAITS_MEMBER(media_ssrc)
  IPC_STRUCT_TRAITS_MEMBER(ack_frame_id)
  IPC_STRUCT_TRAITS_MEMBER(target_delay_ms)
  IPC_STRUCT_TRAITS_MEMBER(missing_frames_and_packets)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::cast::RtcpRttReport)
  IPC_STRUCT_TRAITS_MEMBER(rtt)
  IPC_STRUCT_TRAITS_MEMBER(avg_rtt)
  IPC_STRUCT_TRAITS_MEMBER(min_rtt)
  IPC_STRUCT_TRAITS_MEMBER(max_rtt)
IPC_STRUCT_TRAITS_END()

// Cast messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL3(CastMsg_Rtt,
                     int32 /* channel_id */,
                     uint32 /* ssrc */,
                     media::cast::RtcpRttReport /* rtt_report */)

IPC_MESSAGE_CONTROL3(CastMsg_RtcpCastMessage,
                     int32 /* channel_id */,
                     uint32 /* ssrc */,
                     media::cast::RtcpCastMessage /* cast_message */)

IPC_MESSAGE_CONTROL2(
    CastMsg_NotifyStatusChange,
    int32 /* channel_id */,
    media::cast::CastTransportStatus /* status */)

IPC_MESSAGE_CONTROL3(CastMsg_RawEvents,
                     int32 /* channel_id */,
                     std::vector<media::cast::PacketEvent> /* packet_events */,
                     std::vector<media::cast::FrameEvent> /* frame_events */)

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

IPC_MESSAGE_CONTROL4(
    CastHostMsg_SendSenderReport,
    int32 /* channel_id */,
    uint32 /* ssrc */,
    base::TimeTicks /* current_time */,
    uint32 /* current_time_as_rtp_timestamp */)

IPC_MESSAGE_CONTROL3(
    CastHostMsg_CancelSendingFrames,
    int32 /* channel_id */,
    uint32 /* ssrc */,
    std::vector<uint32> /* frame_ids */)

IPC_MESSAGE_CONTROL3(
    CastHostMsg_ResendFrameForKickstart,
    int32 /* channel_id */,
    uint32 /* ssrc */,
    uint32 /* frame_id */)

IPC_MESSAGE_CONTROL2(
    CastHostMsg_New,
    int32 /* channel_id */,
    net::IPEndPoint /*remote_end_point*/)

IPC_MESSAGE_CONTROL1(
    CastHostMsg_Delete,
    int32 /* channel_id */)
