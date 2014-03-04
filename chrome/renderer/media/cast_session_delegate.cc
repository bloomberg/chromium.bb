// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender.h"

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

static base::LazyInstance<CastThreads> g_cast_threads =
    LAZY_INSTANCE_INITIALIZER;

namespace {

// Allow 10MB for serialized video / audio event logs.
const int kMaxSerializedBytes = 9000000;

// Allow about 9MB for video event logs. Assume serialized log data for
// each frame will take up to 150 bytes.
const int kMaxVideoEventEntries = kMaxSerializedBytes / 150;

// Allow about 9MB for audio event logs. Assume serialized log data for
// each frame will take up to 75 bytes.
const int kMaxAudioEventEntries = kMaxSerializedBytes / 75;

}  // namespace

CastSessionDelegate::CastSessionDelegate()
    : transport_configured_(false),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()),
      weak_factory_(this) {
  DCHECK(io_message_loop_proxy_);
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (audio_event_subscriber_.get()) {
    cast_environment_->Logging()->RemoveRawEventSubscriber(
        audio_event_subscriber_.get());
  }
  if (video_event_subscriber_.get()) {
    cast_environment_->Logging()->RemoveRawEventSubscriber(
        video_event_subscriber_.get());
  }
}

void CastSessionDelegate::Initialize() {
  if (cast_environment_)
    return;  // Already initialized.

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // There's no need to decode so no thread assigned for decoding.
  // Logging: enable raw events and stats collection.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::MessageLoopProxy::current(),
      g_cast_threads.Get().GetAudioEncodeMessageLoopProxy(),
      NULL,
      g_cast_threads.Get().GetVideoEncodeMessageLoopProxy(),
      NULL,
      base::MessageLoopProxy::current(),
      media::cast::GetLoggingConfigWithRawEventsAndStatsEnabled());
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  audio_config_.reset(new AudioSenderConfig(config));
  video_frame_input_available_callback_ = callback;
  StartSendingInternal();
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  audio_frame_input_available_callback_ = callback;

  video_config_.reset(new VideoSenderConfig(config));
  StartSendingInternal();
}

void CastSessionDelegate::StartUDP(const net::IPEndPoint& local_endpoint,
                                   const net::IPEndPoint& remote_endpoint) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  transport_configured_ = true;
  local_endpoint_ = local_endpoint;
  remote_endpoint_ = remote_endpoint;
  StartSendingInternal();
}

void CastSessionDelegate::ToggleLogging(bool is_audio,
                                        bool enable) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (enable) {
    if (is_audio) {
      if (audio_event_subscriber_.get())
        return;
      audio_event_subscriber_.reset(new media::cast::EncodingEventSubscriber(
          media::cast::AUDIO_EVENT, kMaxAudioEventEntries));
      cast_environment_->Logging()->AddRawEventSubscriber(
          audio_event_subscriber_.get());
    } else {
      if (video_event_subscriber_.get())
        return;
      video_event_subscriber_.reset(new media::cast::EncodingEventSubscriber(
          media::cast::VIDEO_EVENT, kMaxVideoEventEntries));
      cast_environment_->Logging()->AddRawEventSubscriber(
          video_event_subscriber_.get());
    }
  } else {
    if (is_audio) {
      if (!audio_event_subscriber_.get())
        return;
      cast_environment_->Logging()->RemoveRawEventSubscriber(
          audio_event_subscriber_.get());
      audio_event_subscriber_.reset();
    } else {
      if (!video_event_subscriber_.get())
        return;
      cast_environment_->Logging()->RemoveRawEventSubscriber(
          video_event_subscriber_.get());
      video_event_subscriber_.reset();
    }
  }
}

void CastSessionDelegate::GetEventLogsAndReset(
    bool is_audio, const EventLogsCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  media::cast::EncodingEventSubscriber* subscriber = is_audio ?
      audio_event_subscriber_.get() : video_event_subscriber_.get();
  if (!subscriber) {
    callback.Run(make_scoped_ptr(new std::string).Pass());
    return;
  }

  media::cast::FrameEventMap frame_events;
  media::cast::PacketEventMap packet_events;
  media::cast::RtpTimestamp rtp_timestamp;

  subscriber->GetEventsAndReset(&frame_events, &packet_events, &rtp_timestamp);

  media::cast::LogSerializer log_serializer(kMaxSerializedBytes);
  bool success = log_serializer.SerializeEventsForStream(
      is_audio, frame_events, packet_events, rtp_timestamp);

  if (!success) {
    VLOG(2) << "Failed to serialize event log.";
    callback.Run(make_scoped_ptr(new std::string).Pass());
    return;
  }

  scoped_ptr<std::string> serialized_log =
      log_serializer.GetSerializedLogAndReset();
  DVLOG(2) << "Serialized log length: " << serialized_log->size();

  callback.Run(serialized_log.Pass());
}

void CastSessionDelegate::StatusNotificationCB(
    media::cast::transport::CastTransportStatus unused_status) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  // TODO(hubbe): Call javascript UDPTransport error function.
}

void CastSessionDelegate::StartSendingInternal() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // No transport, wait.
  if (!transport_configured_)
    return;

  // No audio or video, wait.
  if (!audio_config_ || !video_config_)
    return;

  Initialize();

  // Rationale for using unretained: The callback cannot be called after the
  // destruction of CastTransportSenderIPC, and they both share the same thread.
  cast_transport_.reset(new CastTransportSenderIPC(
      local_endpoint_,
      remote_endpoint_,
      base::Bind(&CastSessionDelegate::StatusNotificationCB,
                 base::Unretained(this))));

  // TODO(hubbe): set config.aes_key and config.aes_iv_mask.
  if (audio_config_) {
    media::cast::transport::CastTransportAudioConfig config;
    config.base.ssrc = audio_config_->sender_ssrc;
    config.codec = audio_config_->codec;
    config.base.rtp_config = audio_config_->rtp_config;
    config.frequency = audio_config_->frequency;
    config.channels = audio_config_->channels;
    cast_transport_->InitializeAudio(config);
  }
  if (video_config_) {
    media::cast::transport::CastTransportVideoConfig config;
    config.base.ssrc = video_config_->sender_ssrc;
    config.codec = video_config_->codec;
    config.base.rtp_config = video_config_->rtp_config;
    cast_transport_->InitializeVideo(config);
  }

  cast_sender_.reset(CastSender::CreateCastSender(
      cast_environment_,
      audio_config_.get(),
      video_config_.get(),
      NULL,  // GPU.
      base::Bind(&CastSessionDelegate::InitializationResult,
                 weak_factory_.GetWeakPtr()),
      cast_transport_.get()));
  cast_transport_->SetPacketReceiver(cast_sender_->packet_receiver());
}

void CastSessionDelegate::InitializationResult(
    media::cast::CastInitializationStatus result) const {
  DCHECK(cast_sender_);

  // TODO(pwestin): handle the error codes.
  if (result == media::cast::STATUS_INITIALIZED) {
    if (!audio_frame_input_available_callback_.is_null()) {
      audio_frame_input_available_callback_.Run(cast_sender_->frame_input());
    }
    if (!video_frame_input_available_callback_.is_null()) {
      video_frame_input_available_callback_.Run(cast_sender_->frame_input());
    }
  }
}

