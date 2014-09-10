// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_sender.h"

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

static base::LazyInstance<CastThreads> g_cast_threads =
    LAZY_INSTANCE_INITIALIZER;

CastSessionDelegate::CastSessionDelegate()
    : io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()),
      weak_factory_(this) {
  DCHECK(io_message_loop_proxy_.get());
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const AudioFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  audio_frame_input_available_callback_ = callback;
  cast_sender_->InitializeAudio(
      config,
      base::Bind(&CastSessionDelegate::InitializationResultCB,
                 weak_factory_.GetWeakPtr(), error_callback));
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const VideoFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback,
    const media::cast::CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const media::cast::CreateVideoEncodeMemoryCallback&
        create_video_encode_mem_cb) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  video_frame_input_available_callback_ = callback;

  cast_sender_->InitializeVideo(
      config,
      base::Bind(&CastSessionDelegate::InitializationResultCB,
                 weak_factory_.GetWeakPtr(), error_callback),
      create_vea_cb,
      create_video_encode_mem_cb);
}

void CastSessionDelegate::StartUDP(const net::IPEndPoint& remote_endpoint,
                                   scoped_ptr<base::DictionaryValue> options) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::MessageLoopProxy::current(),
      g_cast_threads.Get().GetAudioEncodeMessageLoopProxy(),
      g_cast_threads.Get().GetVideoEncodeMessageLoopProxy());

  event_subscribers_.reset(
      new media::cast::RawEventSubscriberBundle(cast_environment_));

  // Rationale for using unretained: The callback cannot be called after the
  // destruction of CastTransportSenderIPC, and they both share the same thread.
  cast_transport_.reset(new CastTransportSenderIPC(
      remote_endpoint,
      options.Pass(),
      base::Bind(&CastSessionDelegate::StatusNotificationCB,
                 base::Unretained(this)),
      base::Bind(&CastSessionDelegate::LogRawEvents, base::Unretained(this))));

  cast_sender_ = CastSender::Create(cast_environment_, cast_transport_.get());
}

void CastSessionDelegate::ToggleLogging(bool is_audio, bool enable) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (!event_subscribers_.get())
    return;

  if (enable)
    event_subscribers_->AddEventSubscribers(is_audio);
  else
    event_subscribers_->RemoveEventSubscribers(is_audio);
}

void CastSessionDelegate::GetEventLogsAndReset(
    bool is_audio,
    const std::string& extra_data,
    const EventLogsCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!event_subscribers_.get()) {
    callback.Run(make_scoped_ptr(new base::BinaryValue).Pass());
    return;
  }

  media::cast::EncodingEventSubscriber* subscriber =
      event_subscribers_->GetEncodingEventSubscriber(is_audio);
  if (!subscriber) {
    callback.Run(make_scoped_ptr(new base::BinaryValue).Pass());
    return;
  }

  media::cast::proto::LogMetadata metadata;
  media::cast::FrameEventList frame_events;
  media::cast::PacketEventList packet_events;

  subscriber->GetEventsAndReset(&metadata, &frame_events, &packet_events);

  if (!extra_data.empty())
    metadata.set_extra_data(extra_data);
  media::cast::proto::GeneralDescription* gen_desc =
      metadata.mutable_general_description();
  chrome::VersionInfo version_info;
  gen_desc->set_product(version_info.Name());
  gen_desc->set_product_version(version_info.Version());
  gen_desc->set_os(version_info.OSType());

  scoped_ptr<char[]> serialized_log(new char[media::cast::kMaxSerializedBytes]);
  int output_bytes;
  bool success = media::cast::SerializeEvents(metadata,
                                              frame_events,
                                              packet_events,
                                              true,
                                              media::cast::kMaxSerializedBytes,
                                              serialized_log.get(),
                                              &output_bytes);

  if (!success) {
    VLOG(2) << "Failed to serialize event log.";
    callback.Run(make_scoped_ptr(new base::BinaryValue).Pass());
    return;
  }

  DVLOG(2) << "Serialized log length: " << output_bytes;

  scoped_ptr<base::BinaryValue> blob(
      new base::BinaryValue(serialized_log.Pass(), output_bytes));
  callback.Run(blob.Pass());
}

void CastSessionDelegate::GetStatsAndReset(bool is_audio,
                                           const StatsCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!event_subscribers_.get()) {
    callback.Run(make_scoped_ptr(new base::DictionaryValue).Pass());
    return;
  }

  media::cast::StatsEventSubscriber* subscriber =
      event_subscribers_->GetStatsEventSubscriber(is_audio);
  if (!subscriber) {
    callback.Run(make_scoped_ptr(new base::DictionaryValue).Pass());
    return;
  }

  scoped_ptr<base::DictionaryValue> stats = subscriber->GetStats();
  subscriber->Reset();

  callback.Run(stats.Pass());
}

void CastSessionDelegate::StatusNotificationCB(
    media::cast::CastTransportStatus unused_status) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  // TODO(hubbe): Call javascript UDPTransport error function.
}

void CastSessionDelegate::InitializationResultCB(
    const ErrorCallback& error_callback,
    media::cast::CastInitializationStatus result) const {
  DCHECK(cast_sender_);

  switch (result) {
    case media::cast::STATUS_AUDIO_INITIALIZED:
      audio_frame_input_available_callback_.Run(
          cast_sender_->audio_frame_input());
      break;
    case media::cast::STATUS_VIDEO_INITIALIZED:
      video_frame_input_available_callback_.Run(
          cast_sender_->video_frame_input());
      break;
    case media::cast::STATUS_INVALID_CAST_ENVIRONMENT:
      error_callback.Run("Invalid cast environment.");
      break;
    case media::cast::STATUS_INVALID_CRYPTO_CONFIGURATION:
      error_callback.Run("Invalid encryption keys.");
      break;
    case media::cast::STATUS_UNSUPPORTED_AUDIO_CODEC:
      error_callback.Run("Audio codec not supported.");
      break;
    case media::cast::STATUS_UNSUPPORTED_VIDEO_CODEC:
      error_callback.Run("Video codec not supported.");
      break;
    case media::cast::STATUS_INVALID_AUDIO_CONFIGURATION:
      error_callback.Run("Invalid audio configuration.");
      break;
    case media::cast::STATUS_INVALID_VIDEO_CONFIGURATION:
      error_callback.Run("Invalid video configuration.");
      break;
    case media::cast::STATUS_HW_VIDEO_ENCODER_NOT_SUPPORTED:
      error_callback.Run("Hardware video encoder not supported.");
      break;
    case media::cast::STATUS_AUDIO_UNINITIALIZED:
    case media::cast::STATUS_VIDEO_UNINITIALIZED:
      NOTREACHED() << "Not an error.";
      break;
  }
}

void CastSessionDelegate::LogRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  for (std::vector<media::cast::PacketEvent>::const_iterator it =
           packet_events.begin();
       it != packet_events.end();
       ++it) {
    cast_environment_->Logging()->InsertPacketEvent(it->timestamp,
                                                    it->type,
                                                    it->media_type,
                                                    it->rtp_timestamp,
                                                    it->frame_id,
                                                    it->packet_id,
                                                    it->max_packet_id,
                                                    it->size);
  }
  for (std::vector<media::cast::FrameEvent>::const_iterator it =
           frame_events.begin();
       it != frame_events.end();
       ++it) {
    if (it->type == media::cast::FRAME_PLAYOUT) {
      cast_environment_->Logging()->InsertFrameEventWithDelay(
          it->timestamp,
          it->type,
          it->media_type,
          it->rtp_timestamp,
          it->frame_id,
          it->delay_delta);
    } else {
      cast_environment_->Logging()->InsertFrameEvent(
          it->timestamp,
          it->type,
          it->media_type,
          it->rtp_timestamp,
          it->frame_id);
    }
  }
}
