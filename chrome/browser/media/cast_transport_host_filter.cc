// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_transport_host_filter.h"

#include "base/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/cast_messages.h"
#include "components/net_log/chrome_net_log.h"
#include "content/public/browser/power_save_blocker.h"
#include "media/cast/net/cast_transport_sender.h"

namespace {

// How often to send raw events.
const int kSendRawEventsIntervalSecs = 1;

}

namespace cast {

CastTransportHostFilter::CastTransportHostFilter()
    : BrowserMessageFilter(CastMsgStart),
      weak_factory_(this) {}

CastTransportHostFilter::~CastTransportHostFilter() {}

bool CastTransportHostFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastTransportHostFilter, message)
    IPC_MESSAGE_HANDLER(CastHostMsg_New, OnNew)
    IPC_MESSAGE_HANDLER(CastHostMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeAudio, OnInitializeAudio)
    IPC_MESSAGE_HANDLER(CastHostMsg_InitializeVideo, OnInitializeVideo)
    IPC_MESSAGE_HANDLER(CastHostMsg_InsertFrame, OnInsertFrame)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendSenderReport,
                        OnSendSenderReport)
    IPC_MESSAGE_HANDLER(CastHostMsg_ResendFrameForKickstart,
                        OnResendFrameForKickstart)
    IPC_MESSAGE_HANDLER(CastHostMsg_CancelSendingFrames,
                        OnCancelSendingFrames)
    IPC_MESSAGE_HANDLER(CastHostMsg_AddValidSsrc,
                        OnAddValidSsrc)
    IPC_MESSAGE_HANDLER(CastHostMsg_SendRtcpFromRtpReceiver,
                        OnSendRtcpFromRtpReceiver)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CastTransportHostFilter::ReceivedPacket(
    int32_t channel_id,
    scoped_ptr<media::cast::Packet> packet) {
  Send(new CastMsg_ReceivedPacket(channel_id, *packet));
}

void CastTransportHostFilter::NotifyStatusChange(
    int32_t channel_id,
    media::cast::CastTransportStatus status) {
  Send(new CastMsg_NotifyStatusChange(channel_id, status));
}

void CastTransportHostFilter::SendRawEvents(
    int32_t channel_id,
    scoped_ptr<std::vector<media::cast::FrameEvent>> frame_events,
    scoped_ptr<std::vector<media::cast::PacketEvent>> packet_events) {
  if (frame_events->empty() && packet_events->empty())
    return;
  Send(new CastMsg_RawEvents(channel_id, *packet_events, *frame_events));
}

void CastTransportHostFilter::SendRtt(int32_t channel_id,
                                      uint32_t ssrc,
                                      base::TimeDelta rtt) {
  Send(new CastMsg_Rtt(channel_id, ssrc, rtt));
}

void CastTransportHostFilter::SendCastMessage(
    int32_t channel_id,
    uint32_t ssrc,
    const media::cast::RtcpCastMessage& cast_message) {
  Send(new CastMsg_RtcpCastMessage(channel_id, ssrc, cast_message));
}

void CastTransportHostFilter::OnNew(int32_t channel_id,
                                    const net::IPEndPoint& local_end_point,
                                    const net::IPEndPoint& remote_end_point,
                                    const base::DictionaryValue& options) {
  if (!power_save_blocker_) {
    DVLOG(1) << ("Preventing the application from being suspended while one or "
                 "more transports are active for Cast Streaming.");
    power_save_blocker_ = content::PowerSaveBlocker::Create(
        content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
        content::PowerSaveBlocker::kReasonOther,
        "Cast is streaming content to a remote receiver");
  }

  if (id_map_.Lookup(channel_id)) {
    id_map_.Remove(channel_id);
  }

  scoped_ptr<media::cast::CastTransportSender> sender =
      media::cast::CastTransportSender::Create(
          g_browser_process->net_log(),
          &clock_,
          local_end_point,
          remote_end_point,
          make_scoped_ptr(options.DeepCopy()),
          base::Bind(&CastTransportHostFilter::NotifyStatusChange,
                     weak_factory_.GetWeakPtr(),
                     channel_id),
          base::Bind(&CastTransportHostFilter::SendRawEvents,
                     weak_factory_.GetWeakPtr(),
                     channel_id),
          base::TimeDelta::FromSeconds(kSendRawEventsIntervalSecs),
          base::Bind(&CastTransportHostFilter::ReceivedPacket,
                     weak_factory_.GetWeakPtr(),
                     channel_id),
          base::ThreadTaskRunnerHandle::Get());
  id_map_.AddWithID(sender.release(), channel_id);
}

void CastTransportHostFilter::OnDelete(int32_t channel_id) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    id_map_.Remove(channel_id);
  } else {
    DVLOG(1) << "CastTransportHostFilter::Delete called "
             << "on non-existing channel";
  }

  if (id_map_.IsEmpty()) {
    DVLOG_IF(1, power_save_blocker_) <<
        ("Releasing the block on application suspension since no transports "
         "are active anymore for Cast Streaming.");
    power_save_blocker_.reset();
  }
}

void CastTransportHostFilter::OnInitializeAudio(
    int32_t channel_id,
    const media::cast::CastTransportRtpConfig& config) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->InitializeAudio(
        config,
        base::Bind(&CastTransportHostFilter::SendCastMessage,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc),
        base::Bind(&CastTransportHostFilter::SendRtt,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc));
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInitializeAudio on non-existing channel";
  }
}

void CastTransportHostFilter::OnInitializeVideo(
    int32_t channel_id,
    const media::cast::CastTransportRtpConfig& config) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->InitializeVideo(
        config,
        base::Bind(&CastTransportHostFilter::SendCastMessage,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc),
        base::Bind(&CastTransportHostFilter::SendRtt,
                   weak_factory_.GetWeakPtr(),
                   channel_id, config.ssrc));
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInitializeVideo on non-existing channel";
  }
}

void CastTransportHostFilter::OnInsertFrame(
    int32_t channel_id,
    uint32_t ssrc,
    const media::cast::EncodedFrame& frame) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->InsertFrame(ssrc, frame);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnInsertFrame on non-existing channel";
  }
}

void CastTransportHostFilter::OnCancelSendingFrames(
    int32_t channel_id,
    uint32_t ssrc,
    const std::vector<uint32_t>& frame_ids) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->CancelSendingFrames(ssrc, frame_ids);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnCancelSendingFrames "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnResendFrameForKickstart(int32_t channel_id,
                                                        uint32_t ssrc,
                                                        uint32_t frame_id) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->ResendFrameForKickstart(ssrc, frame_id);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnResendFrameForKickstart "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendSenderReport(
    int32_t channel_id,
    uint32_t ssrc,
    base::TimeTicks current_time,
    media::cast::RtpTimeTicks current_time_as_rtp_timestamp) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->SendSenderReport(ssrc,
                             current_time,
                             current_time_as_rtp_timestamp);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendSenderReport "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnAddValidSsrc(int32_t channel_id,
                                             uint32_t ssrc) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->AddValidSsrc(ssrc);
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnAddValidSsrc "
        << "on non-existing channel";
  }
}

void CastTransportHostFilter::OnSendRtcpFromRtpReceiver(
    int32_t channel_id,
    const media::cast::SendRtcpFromRtpReceiver_Params& params) {
  media::cast::CastTransportSender* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->SendRtcpFromRtpReceiver(params.ssrc,
                                    params.sender_ssrc,
                                    params.time_data,
                                    params.cast_message.get(),
                                    params.target_delay,
                                    params.rtcp_events.get(),
                                    params.rtp_receiver_statistics.get());
  } else {
    DVLOG(1)
        << "CastTransportHostFilter::OnSendRtcpFromRtpReceiver "
        << "on non-existing channel";
  }
}


}  // namespace cast
