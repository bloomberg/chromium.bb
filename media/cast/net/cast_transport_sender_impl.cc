// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/cast_transport_sender_impl.h"

#include "base/single_thread_task_runner.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/udp_transport.h"
#include "net/base/net_util.h"

namespace media {
namespace cast {

scoped_ptr<CastTransportSender> CastTransportSender::Create(
    net::NetLog* net_log,
    base::TickClock* clock,
    const net::IPEndPoint& remote_end_point,
    const CastTransportStatusCallback& status_callback,
    const BulkRawEventsCallback& raw_events_callback,
    base::TimeDelta raw_events_callback_interval,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner) {
  return scoped_ptr<CastTransportSender>(
      new CastTransportSenderImpl(net_log,
                                  clock,
                                  remote_end_point,
                                  status_callback,
                                  raw_events_callback,
                                  raw_events_callback_interval,
                                  transport_task_runner.get(),
                                  NULL));
}

PacketReceiverCallback CastTransportSender::PacketReceiverForTesting() {
  return PacketReceiverCallback();
}

CastTransportSenderImpl::CastTransportSenderImpl(
    net::NetLog* net_log,
    base::TickClock* clock,
    const net::IPEndPoint& remote_end_point,
    const CastTransportStatusCallback& status_callback,
    const BulkRawEventsCallback& raw_events_callback,
    base::TimeDelta raw_events_callback_interval,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    PacketSender* external_transport)
    : clock_(clock),
      status_callback_(status_callback),
      transport_task_runner_(transport_task_runner),
      transport_(external_transport ? NULL
                                    : new UdpTransport(net_log,
                                                       transport_task_runner,
                                                       net::IPEndPoint(),
                                                       remote_end_point,
                                                       status_callback)),
      pacer_(clock,
             &logging_,
             external_transport ? external_transport : transport_.get(),
             transport_task_runner),
      raw_events_callback_(raw_events_callback),
      raw_events_callback_interval_(raw_events_callback_interval),
      weak_factory_(this) {
  DCHECK(clock_);
  if (!raw_events_callback_.is_null()) {
    DCHECK(raw_events_callback_interval > base::TimeDelta());
    event_subscriber_.reset(new SimpleEventSubscriber);
    logging_.AddRawEventSubscriber(event_subscriber_.get());
    transport_task_runner->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CastTransportSenderImpl::SendRawEvents,
                   weak_factory_.GetWeakPtr()),
        raw_events_callback_interval);
  }
  if (transport_) {
    // The default DSCP value for cast is AF41. Which gives it a higher
    // priority over other traffic.
    transport_->SetDscp(net::DSCP_AF41);
    transport_->StartReceiving(
        base::Bind(&CastTransportSenderImpl::OnReceivedPacket,
                   weak_factory_.GetWeakPtr()));
  }
}

CastTransportSenderImpl::~CastTransportSenderImpl() {
  if (event_subscriber_.get())
    logging_.RemoveRawEventSubscriber(event_subscriber_.get());
}

void CastTransportSenderImpl::InitializeAudio(
    const CastTransportRtpConfig& config,
    const RtcpCastMessageCallback& cast_message_cb,
    const RtcpRttCallback& rtt_cb) {
  LOG_IF(WARNING, config.aes_key.empty() || config.aes_iv_mask.empty())
      << "Unsafe to send audio with encryption DISABLED.";
  if (!audio_encryptor_.Initialize(config.aes_key, config.aes_iv_mask)) {
    status_callback_.Run(TRANSPORT_AUDIO_UNINITIALIZED);
    return;
  }

  audio_sender_.reset(new RtpSender(clock_, transport_task_runner_, &pacer_));
  if (audio_sender_->Initialize(config)) {
    // Audio packets have a higher priority.
    pacer_.RegisterAudioSsrc(config.ssrc);
    pacer_.RegisterPrioritySsrc(config.ssrc);
    status_callback_.Run(TRANSPORT_AUDIO_INITIALIZED);
  } else {
    audio_sender_.reset();
    status_callback_.Run(TRANSPORT_AUDIO_UNINITIALIZED);
    return;
  }

  audio_rtcp_session_.reset(
      new Rtcp(cast_message_cb,
               rtt_cb,
               base::Bind(&CastTransportSenderImpl::OnReceivedLogMessage,
                          weak_factory_.GetWeakPtr(), AUDIO_EVENT),
               clock_,
               &pacer_,
               config.ssrc,
               config.feedback_ssrc));
  pacer_.RegisterAudioSsrc(config.ssrc);
  status_callback_.Run(TRANSPORT_AUDIO_INITIALIZED);
}

void CastTransportSenderImpl::InitializeVideo(
    const CastTransportRtpConfig& config,
    const RtcpCastMessageCallback& cast_message_cb,
    const RtcpRttCallback& rtt_cb) {
  LOG_IF(WARNING, config.aes_key.empty() || config.aes_iv_mask.empty())
      << "Unsafe to send video with encryption DISABLED.";
  if (!video_encryptor_.Initialize(config.aes_key, config.aes_iv_mask)) {
    status_callback_.Run(TRANSPORT_VIDEO_UNINITIALIZED);
    return;
  }

  video_sender_.reset(new RtpSender(clock_, transport_task_runner_, &pacer_));
  if (!video_sender_->Initialize(config)) {
    video_sender_.reset();
    status_callback_.Run(TRANSPORT_VIDEO_UNINITIALIZED);
    return;
  }

  video_rtcp_session_.reset(
      new Rtcp(cast_message_cb,
               rtt_cb,
               base::Bind(&CastTransportSenderImpl::OnReceivedLogMessage,
                          weak_factory_.GetWeakPtr(), VIDEO_EVENT),
               clock_,
               &pacer_,
               config.ssrc,
               config.feedback_ssrc));
  pacer_.RegisterVideoSsrc(config.ssrc);
  status_callback_.Run(TRANSPORT_VIDEO_INITIALIZED);
}

namespace {
void EncryptAndSendFrame(const EncodedFrame& frame,
                         TransportEncryptionHandler* encryptor,
                         RtpSender* sender) {
  if (encryptor->is_activated()) {
    EncodedFrame encrypted_frame;
    frame.CopyMetadataTo(&encrypted_frame);
    if (encryptor->Encrypt(frame.frame_id, frame.data, &encrypted_frame.data)) {
      sender->SendFrame(encrypted_frame);
    } else {
      LOG(ERROR) << "Encryption failed.  Not sending frame with ID "
                 << frame.frame_id;
    }
  } else {
    sender->SendFrame(frame);
  }
}
}  // namespace

void CastTransportSenderImpl::InsertCodedAudioFrame(
    const EncodedFrame& audio_frame) {
  DCHECK(audio_sender_) << "Audio sender uninitialized";
  EncryptAndSendFrame(audio_frame, &audio_encryptor_, audio_sender_.get());
}

void CastTransportSenderImpl::InsertCodedVideoFrame(
    const EncodedFrame& video_frame) {
  DCHECK(video_sender_) << "Video sender uninitialized";
  EncryptAndSendFrame(video_frame, &video_encryptor_, video_sender_.get());
}

void CastTransportSenderImpl::SendSenderReport(
    uint32 ssrc,
    base::TimeTicks current_time,
    uint32 current_time_as_rtp_timestamp) {
  if (audio_sender_ && ssrc == audio_sender_->ssrc()) {
    audio_rtcp_session_->SendRtcpFromRtpSender(
        current_time, current_time_as_rtp_timestamp,
        audio_sender_->send_packet_count(), audio_sender_->send_octet_count());
  } else if (video_sender_ && ssrc == video_sender_->ssrc()) {
    video_rtcp_session_->SendRtcpFromRtpSender(
        current_time, current_time_as_rtp_timestamp,
        video_sender_->send_packet_count(), video_sender_->send_octet_count());
  } else {
    NOTREACHED() << "Invalid request for sending RTCP packet.";
  }
}

void CastTransportSenderImpl::ResendPackets(
    bool is_audio,
    const MissingFramesAndPacketsMap& missing_packets,
    bool cancel_rtx_if_not_in_list,
    base::TimeDelta dedupe_window) {
  if (is_audio) {
    DCHECK(audio_sender_) << "Audio sender uninitialized";
    audio_sender_->ResendPackets(missing_packets,
                                 cancel_rtx_if_not_in_list,
                                 dedupe_window);
  } else {
    DCHECK(video_sender_) << "Video sender uninitialized";
    video_sender_->ResendPackets(missing_packets,
                                 cancel_rtx_if_not_in_list,
                                 dedupe_window);
  }
}

PacketReceiverCallback CastTransportSenderImpl::PacketReceiverForTesting() {
  return base::Bind(&CastTransportSenderImpl::OnReceivedPacket,
                    weak_factory_.GetWeakPtr());
}

void CastTransportSenderImpl::SendRawEvents() {
  DCHECK(event_subscriber_.get());
  DCHECK(!raw_events_callback_.is_null());
  std::vector<PacketEvent> packet_events;
  std::vector<FrameEvent> frame_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);
  event_subscriber_->GetFrameEventsAndReset(&frame_events);
  raw_events_callback_.Run(packet_events, frame_events);

  transport_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CastTransportSenderImpl::SendRawEvents,
                 weak_factory_.GetWeakPtr()),
      raw_events_callback_interval_);
}

void CastTransportSenderImpl::OnReceivedPacket(scoped_ptr<Packet> packet) {
  if (audio_rtcp_session_ &&
      audio_rtcp_session_->IncomingRtcpPacket(&packet->front(),
                                              packet->size())) {
    return;
  }
  if (video_rtcp_session_ &&
      video_rtcp_session_->IncomingRtcpPacket(&packet->front(),
                                              packet->size())) {
    return;
  }
  VLOG(1) << "Stale packet received.";
}

void CastTransportSenderImpl::OnReceivedLogMessage(
    EventMediaType media_type,
    const RtcpReceiverLogMessage& log) {
  // Add received log messages into our log system.
  RtcpReceiverLogMessage::const_iterator it = log.begin();
  for (; it != log.end(); ++it) {
    uint32 rtp_timestamp = it->rtp_timestamp_;

    RtcpReceiverEventLogMessages::const_iterator event_it =
        it->event_log_messages_.begin();
    for (; event_it != it->event_log_messages_.end(); ++event_it) {
      switch (event_it->type) {
        case PACKET_RECEIVED:
          logging_.InsertPacketEvent(
              event_it->event_timestamp, event_it->type,
              media_type, rtp_timestamp,
              kFrameIdUnknown, event_it->packet_id, 0, 0);
          break;
        case FRAME_ACK_SENT:
        case FRAME_DECODED:
          logging_.InsertFrameEvent(
              event_it->event_timestamp, event_it->type, media_type,
              rtp_timestamp, kFrameIdUnknown);
          break;
        case FRAME_PLAYOUT:
          logging_.InsertFrameEventWithDelay(
              event_it->event_timestamp, event_it->type, media_type,
              rtp_timestamp, kFrameIdUnknown, event_it->delay_delta);
          break;
        default:
          VLOG(2) << "Received log message via RTCP that we did not expect: "
                  << static_cast<int>(event_it->type);
          break;
      }
    }
  }
}

}  // namespace cast
}  // namespace media
