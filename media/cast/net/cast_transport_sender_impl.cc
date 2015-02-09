// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/cast_transport_sender_impl.h"

#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/udp_transport.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace media {
namespace cast {

namespace {

// See header file for what these mean.
const char kOptionDscp[] = "DSCP";
#if defined(OS_WIN)
const char kOptionNonBlockingIO[] = "non_blocking_io";
#endif
const char kOptionPacerTargetBurstSize[] = "pacer_target_burst_size";
const char kOptionPacerMaxBurstSize[] = "pacer_max_burst_size";
const char kOptionSendBufferMinSize[] = "send_buffer_min_size";
const char kOptionWifiDisableScan[] = "disable_wifi_scan";
const char kOptionWifiMediaStreamingMode[] = "media_streaming_mode";

int LookupOptionWithDefault(const base::DictionaryValue& options,
                            const std::string& path,
                            int default_value) {
  int ret;
  if (options.GetInteger(path, &ret)) {
    return ret;
  } else {
    return default_value;
  }
};

int32 GetTransportSendBufferSize(const base::DictionaryValue& options) {
  // Socket send buffer size needs to be at least greater than one burst
  // size.
  int32 max_burst_size =
      LookupOptionWithDefault(options, kOptionPacerMaxBurstSize,
                              kMaxBurstSize) * kMaxIpPacketSize;
  int32 min_send_buffer_size =
      LookupOptionWithDefault(options, kOptionSendBufferMinSize, 0);
  return std::max(max_burst_size, min_send_buffer_size);
}

}  // namespace

scoped_ptr<CastTransportSender> CastTransportSender::Create(
    net::NetLog* net_log,
    base::TickClock* clock,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    scoped_ptr<base::DictionaryValue> options,
    const CastTransportStatusCallback& status_callback,
    const BulkRawEventsCallback& raw_events_callback,
    base::TimeDelta raw_events_callback_interval,
    const PacketReceiverCallback& packet_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner) {
  return scoped_ptr<CastTransportSender>(
      new CastTransportSenderImpl(net_log,
                                  clock,
                                  local_end_point,
                                  remote_end_point,
                                  options.Pass(),
                                  status_callback,
                                  raw_events_callback,
                                  raw_events_callback_interval,
                                  transport_task_runner.get(),
                                  packet_callback,
                                  NULL));
}

PacketReceiverCallback CastTransportSender::PacketReceiverForTesting() {
  return PacketReceiverCallback();
}

CastTransportSenderImpl::CastTransportSenderImpl(
    net::NetLog* net_log,
    base::TickClock* clock,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    scoped_ptr<base::DictionaryValue> options,
    const CastTransportStatusCallback& status_callback,
    const BulkRawEventsCallback& raw_events_callback,
    base::TimeDelta raw_events_callback_interval,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    const PacketReceiverCallback& packet_callback,
    PacketSender* external_transport)
    : clock_(clock),
      status_callback_(status_callback),
      transport_task_runner_(transport_task_runner),
      transport_(
          external_transport ?
              NULL :
              new UdpTransport(net_log,
                               transport_task_runner,
                               local_end_point,
                               remote_end_point,
                               GetTransportSendBufferSize(*options),
                               status_callback)),
      pacer_(LookupOptionWithDefault(*options,
                                     kOptionPacerTargetBurstSize,
                                     kTargetBurstSize),
             LookupOptionWithDefault(*options,
                                     kOptionPacerMaxBurstSize,
                                     kMaxBurstSize),
             clock,
             &logging_,
             external_transport ? external_transport : transport_.get(),
             transport_task_runner),
      raw_events_callback_(raw_events_callback),
      raw_events_callback_interval_(raw_events_callback_interval),
      last_byte_acked_for_audio_(0),
      packet_callback_(packet_callback),
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
    if (options->HasKey(kOptionDscp)) {
      // The default DSCP value for cast is AF41. Which gives it a higher
      // priority over other traffic.
      transport_->SetDscp(net::DSCP_AF41);
    }
#if defined(OS_WIN)
    if (options->HasKey(kOptionNonBlockingIO)) {
      transport_->UseNonBlockingIO();
    }
#endif
    transport_->StartReceiving(
        base::Bind(&CastTransportSenderImpl::OnReceivedPacket,
                   base::Unretained(this)));
    int wifi_options = 0;
    if (options->HasKey(kOptionWifiDisableScan)) {
      wifi_options |= net::WIFI_OPTIONS_DISABLE_SCAN;
    }
    if (options->HasKey(kOptionWifiMediaStreamingMode)) {
      wifi_options |= net::WIFI_OPTIONS_MEDIA_STREAMING_MODE;
    }
    if (wifi_options) {
      wifi_options_autoreset_ = net::SetWifiOptions(wifi_options);
    }
  }
}

CastTransportSenderImpl::~CastTransportSenderImpl() {
  if (transport_) {
    transport_->StopReceiving();
  }
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

  audio_sender_.reset(new RtpSender(transport_task_runner_, &pacer_));
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
      new Rtcp(base::Bind(&CastTransportSenderImpl::OnReceivedCastMessage,
                          weak_factory_.GetWeakPtr(), config.ssrc,
                          cast_message_cb),
               rtt_cb,
               base::Bind(&CastTransportSenderImpl::OnReceivedLogMessage,
                          weak_factory_.GetWeakPtr(), AUDIO_EVENT),
               clock_,
               &pacer_,
               config.ssrc,
               config.feedback_ssrc));
  pacer_.RegisterAudioSsrc(config.ssrc);
  AddValidSsrc(config.feedback_ssrc);
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

  video_sender_.reset(new RtpSender(transport_task_runner_, &pacer_));
  if (!video_sender_->Initialize(config)) {
    video_sender_.reset();
    status_callback_.Run(TRANSPORT_VIDEO_UNINITIALIZED);
    return;
  }

  video_rtcp_session_.reset(
      new Rtcp(base::Bind(&CastTransportSenderImpl::OnReceivedCastMessage,
                          weak_factory_.GetWeakPtr(), config.ssrc,
                          cast_message_cb),
               rtt_cb,
               base::Bind(&CastTransportSenderImpl::OnReceivedLogMessage,
                          weak_factory_.GetWeakPtr(), VIDEO_EVENT),
               clock_,
               &pacer_,
               config.ssrc,
               config.feedback_ssrc));
  pacer_.RegisterVideoSsrc(config.ssrc);
  AddValidSsrc(config.feedback_ssrc);
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

void CastTransportSenderImpl::InsertFrame(uint32 ssrc,
                                          const EncodedFrame& frame) {
  if (audio_sender_ && ssrc == audio_sender_->ssrc()) {
    EncryptAndSendFrame(frame, &audio_encryptor_, audio_sender_.get());
  } else if (video_sender_ && ssrc == video_sender_->ssrc()) {
    EncryptAndSendFrame(frame, &video_encryptor_, video_sender_.get());
  } else {
    NOTREACHED() << "Invalid InsertFrame call.";
  }
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

void CastTransportSenderImpl::CancelSendingFrames(
    uint32 ssrc,
    const std::vector<uint32>& frame_ids) {
  if (audio_sender_ && ssrc == audio_sender_->ssrc()) {
    audio_sender_->CancelSendingFrames(frame_ids);
  } else if (video_sender_ && ssrc == video_sender_->ssrc()) {
    video_sender_->CancelSendingFrames(frame_ids);
  } else {
    NOTREACHED() << "Invalid request for cancel sending.";
  }
}

void CastTransportSenderImpl::ResendFrameForKickstart(uint32 ssrc,
                                                      uint32 frame_id) {
  if (audio_sender_ && ssrc == audio_sender_->ssrc()) {
    DCHECK(audio_rtcp_session_);
    audio_sender_->ResendFrameForKickstart(
        frame_id,
        audio_rtcp_session_->current_round_trip_time());
  } else if (video_sender_ && ssrc == video_sender_->ssrc()) {
    DCHECK(video_rtcp_session_);
    video_sender_->ResendFrameForKickstart(
        frame_id,
        video_rtcp_session_->current_round_trip_time());
  } else {
    NOTREACHED() << "Invalid request for kickstart.";
  }
}

void CastTransportSenderImpl::ResendPackets(
    uint32 ssrc,
    const MissingFramesAndPacketsMap& missing_packets,
    bool cancel_rtx_if_not_in_list,
    const DedupInfo& dedup_info) {
  if (audio_sender_ && ssrc == audio_sender_->ssrc()) {
    audio_sender_->ResendPackets(missing_packets,
                                 cancel_rtx_if_not_in_list,
                                 dedup_info);
  } else if (video_sender_ && ssrc == video_sender_->ssrc()) {
    video_sender_->ResendPackets(missing_packets,
                                 cancel_rtx_if_not_in_list,
                                 dedup_info);
  } else {
    NOTREACHED() << "Invalid request for retransmission.";
  }
}

PacketReceiverCallback CastTransportSenderImpl::PacketReceiverForTesting() {
  return base::Bind(
      base::IgnoreResult(&CastTransportSenderImpl::OnReceivedPacket),
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

bool CastTransportSenderImpl::OnReceivedPacket(scoped_ptr<Packet> packet) {
  const uint8_t* const data = &packet->front();
  const size_t length = packet->size();
  uint32 ssrc;
  if (Rtcp::IsRtcpPacket(data, length)) {
    ssrc = Rtcp::GetSsrcOfSender(data, length);
  } else if (!RtpParser::ParseSsrc(data, length, &ssrc)) {
    VLOG(1) << "Invalid RTP packet.";
    return false;
  }
  if (valid_ssrcs_.find(ssrc) == valid_ssrcs_.end()) {
    VLOG(1) << "Stale packet received.";
    return false;
  }

  if (audio_rtcp_session_ &&
      audio_rtcp_session_->IncomingRtcpPacket(data, length)) {
    return true;
  }
  if (video_rtcp_session_ &&
      video_rtcp_session_->IncomingRtcpPacket(data, length)) {
    return true;
  }
  if (packet_callback_.is_null()) {
    VLOG(1) << "Stale packet received.";
    return false;
  }
  packet_callback_.Run(packet.Pass());
  return true;
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

void CastTransportSenderImpl::OnReceivedCastMessage(
    uint32 ssrc,
    const RtcpCastMessageCallback& cast_message_cb,
    const RtcpCastMessage& cast_message) {
  if (!cast_message_cb.is_null())
    cast_message_cb.Run(cast_message);

  DedupInfo dedup_info;
  if (audio_sender_ && audio_sender_->ssrc() == ssrc) {
    const int64 acked_bytes =
        audio_sender_->GetLastByteSentForFrame(cast_message.ack_frame_id);
    last_byte_acked_for_audio_ =
        std::max(acked_bytes, last_byte_acked_for_audio_);
  } else if (video_sender_ && video_sender_->ssrc() == ssrc) {
    dedup_info.resend_interval = video_rtcp_session_->current_round_trip_time();

    // Only use audio stream to dedup if there is one.
    if (audio_sender_) {
      dedup_info.last_byte_acked_for_audio = last_byte_acked_for_audio_;
    }
  }

  if (cast_message.missing_frames_and_packets.empty())
    return;

  // This call does two things.
  // 1. Specifies that retransmissions for packets not listed in the set are
  //    cancelled.
  // 2. Specifies a deduplication window. For video this would be the most
  //    recent RTT. For audio there is no deduplication.
  ResendPackets(ssrc,
                cast_message.missing_frames_and_packets,
                true,
                dedup_info);
}

void CastTransportSenderImpl::AddValidSsrc(uint32 ssrc) {
  valid_ssrcs_.insert(ssrc);
}

void CastTransportSenderImpl::SendRtcpFromRtpReceiver(
    uint32 ssrc,
    uint32 sender_ssrc,
    const RtcpTimeData& time_data,
    const RtcpCastMessage* cast_message,
    base::TimeDelta target_delay,
    const ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
    const RtpReceiverStatistics* rtp_receiver_statistics) {
  const Rtcp rtcp(RtcpCastMessageCallback(),
                  RtcpRttCallback(),
                  RtcpLogMessageCallback(),
                  clock_,
                  &pacer_,
                  ssrc,
                  sender_ssrc);
  rtcp.SendRtcpFromRtpReceiver(time_data,
                               cast_message,
                               target_delay,
                               rtcp_events,
                               rtp_receiver_statistics);
}

}  // namespace cast
}  // namespace media
