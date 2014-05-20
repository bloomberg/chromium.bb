// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/cast_transport_sender_impl.h"

#include "base/single_thread_task_runner.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

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
      logging_(),
      pacer_(clock,
             &logging_,
             external_transport ? external_transport : transport_.get(),
             transport_task_runner),
      rtcp_builder_(&pacer_),
      raw_events_callback_(raw_events_callback) {
  if (!raw_events_callback_.is_null()) {
    DCHECK(raw_events_callback_interval > base::TimeDelta());
    event_subscriber_.reset(new SimpleEventSubscriber);
    logging_.AddRawEventSubscriber(event_subscriber_.get());
    raw_events_timer_.Start(FROM_HERE,
                            raw_events_callback_interval,
                            this,
                            &CastTransportSenderImpl::SendRawEvents);
  }
}

CastTransportSenderImpl::~CastTransportSenderImpl() {
  if (event_subscriber_.get())
    logging_.RemoveRawEventSubscriber(event_subscriber_.get());
}

void CastTransportSenderImpl::InitializeAudio(
    const CastTransportAudioConfig& config) {
  pacer_.RegisterAudioSsrc(config.rtp.config.ssrc);
  audio_sender_.reset(new TransportAudioSender(
      config, clock_, transport_task_runner_, &pacer_));
  if (audio_sender_->initialized())
    status_callback_.Run(TRANSPORT_AUDIO_INITIALIZED);
  else
    status_callback_.Run(TRANSPORT_AUDIO_UNINITIALIZED);
}

void CastTransportSenderImpl::InitializeVideo(
    const CastTransportVideoConfig& config) {
  pacer_.RegisterVideoSsrc(config.rtp.config.ssrc);
  video_sender_.reset(new TransportVideoSender(
      config, clock_, transport_task_runner_, &pacer_));
  if (video_sender_->initialized())
    status_callback_.Run(TRANSPORT_VIDEO_INITIALIZED);
  else
    status_callback_.Run(TRANSPORT_VIDEO_UNINITIALIZED);
}

void CastTransportSenderImpl::SetPacketReceiver(
    const PacketReceiverCallback& packet_receiver) {
  transport_->StartReceiving(packet_receiver);
}

void CastTransportSenderImpl::InsertCodedAudioFrame(
    const EncodedFrame& audio_frame) {
  DCHECK(audio_sender_) << "Audio sender uninitialized";
  audio_sender_->SendFrame(audio_frame);
}

void CastTransportSenderImpl::InsertCodedVideoFrame(
    const EncodedFrame& video_frame) {
  DCHECK(video_sender_) << "Video sender uninitialized";
  video_sender_->SendFrame(video_frame);
}

void CastTransportSenderImpl::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    uint32 ntp_seconds,
    uint32 ntp_fraction,
    uint32 rtp_timestamp,
    const RtcpDlrrReportBlock& dlrr,
    uint32 sending_ssrc,
    const std::string& c_name) {
  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = ntp_seconds;
  sender_info.ntp_fraction = ntp_fraction;
  sender_info.rtp_timestamp = rtp_timestamp;
  if (audio_sender_ && audio_sender_->ssrc() == sending_ssrc) {
    sender_info.send_packet_count = audio_sender_->send_packet_count();
    sender_info.send_octet_count = audio_sender_->send_octet_count();
  } else if (video_sender_ && video_sender_->ssrc() == sending_ssrc) {
    sender_info.send_packet_count = video_sender_->send_packet_count();
    sender_info.send_octet_count = video_sender_->send_octet_count();
  } else {
    LOG(ERROR) << "Sending RTCP with an invalid SSRC.";
    return;
  }
  rtcp_builder_.SendRtcpFromRtpSender(
      packet_type_flags, sender_info, dlrr, sending_ssrc, c_name);
}

void CastTransportSenderImpl::ResendPackets(
    bool is_audio,
    const MissingFramesAndPacketsMap& missing_packets) {
  if (is_audio) {
    DCHECK(audio_sender_) << "Audio sender uninitialized";
    audio_sender_->ResendPackets(missing_packets);
  } else {
    DCHECK(video_sender_) << "Video sender uninitialized";
    video_sender_->ResendPackets(missing_packets);
  }
}

void CastTransportSenderImpl::SendRawEvents() {
  DCHECK(event_subscriber_.get());
  DCHECK(!raw_events_callback_.is_null());
  std::vector<PacketEvent> packet_events;
  event_subscriber_->GetPacketEventsAndReset(&packet_events);
  raw_events_callback_.Run(packet_events);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
