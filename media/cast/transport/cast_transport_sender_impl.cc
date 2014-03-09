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
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const CastLoggingConfig& logging_config,
    const CastTransportStatusCallback& status_callback,
    const BulkRawEventsCallback& raw_events_callback,
    base::TimeDelta raw_events_callback_interval,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner) {
  return scoped_ptr<CastTransportSender>(
      new CastTransportSenderImpl(net_log,
                                  clock,
                                  local_end_point,
                                  remote_end_point,
                                  logging_config,
                                  status_callback,
                                  raw_events_callback,
                                  raw_events_callback_interval,
                                  transport_task_runner.get(),
                                  NULL));
}

CastTransportSenderImpl::CastTransportSenderImpl(
    net::NetLog* net_log,
    base::TickClock* clock,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const CastLoggingConfig& logging_config,
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
                                                       local_end_point,
                                                       remote_end_point,
                                                       status_callback)),
      pacer_(clock,
             external_transport ? external_transport : transport_.get(),
             transport_task_runner),
      rtcp_builder_(&pacer_),
      logging_(logging_config),
      raw_events_callback_(raw_events_callback) {
  if (!raw_events_callback_.is_null()) {
    DCHECK(logging_config.enable_raw_data_collection);
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
  audio_sender_.reset(new TransportAudioSender(
      config, clock_, transport_task_runner_, &pacer_));
  if (audio_sender_->initialized())
    status_callback_.Run(TRANSPORT_AUDIO_INITIALIZED);
  else
    status_callback_.Run(TRANSPORT_AUDIO_UNINITIALIZED);
}

void CastTransportSenderImpl::InitializeVideo(
    const CastTransportVideoConfig& config) {
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
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  DCHECK(audio_sender_) << "Audio sender uninitialized";
  audio_sender_->InsertCodedAudioFrame(audio_frame, recorded_time);
}

void CastTransportSenderImpl::InsertCodedVideoFrame(
    const EncodedVideoFrame* video_frame,
    const base::TimeTicks& capture_time) {
  DCHECK(video_sender_) << "Video sender uninitialized";
  video_sender_->InsertCodedVideoFrame(video_frame, capture_time);
}

void CastTransportSenderImpl::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const RtcpSenderInfo& sender_info,
    const RtcpDlrrReportBlock& dlrr,
    const RtcpSenderLogMessage& sender_log,
    uint32 sending_ssrc,
    const std::string& c_name) {
  rtcp_builder_.SendRtcpFromRtpSender(
      packet_type_flags, sender_info, dlrr, sender_log, sending_ssrc, c_name);
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

void CastTransportSenderImpl::SubscribeAudioRtpStatsCallback(
    const CastTransportRtpStatistics& callback) {
  DCHECK(audio_sender_) << "Audio sender uninitialized";
  audio_sender_->SubscribeAudioRtpStatsCallback(callback);
}

void CastTransportSenderImpl::SubscribeVideoRtpStatsCallback(
    const CastTransportRtpStatistics& callback) {
  DCHECK(video_sender_) << "Audio sender uninitialized";
  video_sender_->SubscribeVideoRtpStatsCallback(callback);
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
