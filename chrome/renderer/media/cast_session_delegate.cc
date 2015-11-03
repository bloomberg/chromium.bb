// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "components/version_info/version_info.h"
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

CastSessionDelegateBase::CastSessionDelegateBase()
    : io_task_runner_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()),
      weak_factory_(this) {
  DCHECK(io_task_runner_.get());
#if defined(OS_WIN)
  // Note that this also increases the accuracy of PostDelayTask,
  // which is is very helpful to cast.
  if (!base::Time::ActivateHighResolutionTimer(true)) {
    LOG(WARNING) << "Failed to activate high resolution timers for cast.";
  }
#endif
}

CastSessionDelegateBase::~CastSessionDelegateBase() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
#if defined(OS_WIN)
  base::Time::ActivateHighResolutionTimer(false);
#endif
}

void CastSessionDelegateBase::StartUDP(
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    scoped_ptr<base::DictionaryValue> options,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // TODO(hubbe): Create cast environment in ctor instead.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::ThreadTaskRunnerHandle::Get(),
      g_cast_threads.Get().GetAudioEncodeMessageLoopProxy(),
      g_cast_threads.Get().GetVideoEncodeMessageLoopProxy());

  // Rationale for using unretained: The callback cannot be called after the
  // destruction of CastTransportSenderIPC, and they both share the same thread.
  cast_transport_.reset(new CastTransportSenderIPC(
      local_endpoint,
      remote_endpoint,
      options.Pass(),
      base::Bind(&CastSessionDelegateBase::ReceivePacket,
                 base::Unretained(this)),
      base::Bind(&CastSessionDelegateBase::StatusNotificationCB,
                 base::Unretained(this), error_callback),
      base::Bind(&media::cast::LogEventDispatcher::DispatchBatchOfEvents,
                 base::Unretained(cast_environment_->logger()))));
}

void CastSessionDelegateBase::StatusNotificationCB(
    const ErrorCallback& error_callback,
    media::cast::CastTransportStatus status) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::string error_message;

  switch (status) {
    case media::cast::TRANSPORT_AUDIO_UNINITIALIZED:
    case media::cast::TRANSPORT_VIDEO_UNINITIALIZED:
    case media::cast::TRANSPORT_AUDIO_INITIALIZED:
    case media::cast::TRANSPORT_VIDEO_INITIALIZED:
      return; // Not errors, do nothing.
    case media::cast::TRANSPORT_INVALID_CRYPTO_CONFIG:
      error_callback.Run("Invalid encrypt/decrypt configuration.");
      break;
    case media::cast::TRANSPORT_SOCKET_ERROR:
      error_callback.Run("Socket error.");
      break;
  }
}

CastSessionDelegate::CastSessionDelegate()
    : weak_factory_(this) {
  DCHECK(io_task_runner_.get());
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const AudioFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  audio_frame_input_available_callback_ = callback;
  cast_sender_->InitializeAudio(
      config,
      base::Bind(&CastSessionDelegate::OnOperationalStatusChange,
                 weak_factory_.GetWeakPtr(), true, error_callback));
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const VideoFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback,
    const media::cast::CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const media::cast::CreateVideoEncodeMemoryCallback&
        create_video_encode_mem_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  video_frame_input_available_callback_ = callback;

  cast_sender_->InitializeVideo(
      config,
      base::Bind(&CastSessionDelegate::OnOperationalStatusChange,
                 weak_factory_.GetWeakPtr(), false, error_callback),
      create_vea_cb,
      create_video_encode_mem_cb);
}


void CastSessionDelegate::StartUDP(
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    scoped_ptr<base::DictionaryValue> options,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  CastSessionDelegateBase::StartUDP(local_endpoint,
                                    remote_endpoint,
                                    options.Pass(),
                                    error_callback);
  event_subscribers_.reset(
      new media::cast::RawEventSubscriberBundle(cast_environment_));

  cast_sender_ = CastSender::Create(cast_environment_, cast_transport_.get());
}

void CastSessionDelegate::ToggleLogging(bool is_audio, bool enable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
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
  DCHECK(io_task_runner_->BelongsToCurrentThread());

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
  gen_desc->set_product(version_info::GetProductName());
  gen_desc->set_product_version(version_info::GetVersionNumber());
  gen_desc->set_os(version_info::GetOSType());

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
    DVLOG(2) << "Failed to serialize event log.";
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
  DCHECK(io_task_runner_->BelongsToCurrentThread());

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

void CastSessionDelegate::OnOperationalStatusChange(
    bool is_for_audio,
    const ErrorCallback& error_callback,
    media::cast::OperationalStatus status) {
  DCHECK(cast_sender_);

  switch (status) {
    case media::cast::STATUS_UNINITIALIZED:
    case media::cast::STATUS_CODEC_REINIT_PENDING:
      // Not an error.
      // TODO(miu): As an optimization, signal the client to pause sending more
      // frames until the state becomes STATUS_INITIALIZED again.
      break;
    case media::cast::STATUS_INITIALIZED:
      // Once initialized, run the "frame input available" callback to allow the
      // client to begin sending frames.  If STATUS_INITIALIZED is encountered
      // again, do nothing since this is only an indication that the codec has
      // successfully re-initialized.
      if (is_for_audio) {
        if (!audio_frame_input_available_callback_.is_null()) {
          base::ResetAndReturn(&audio_frame_input_available_callback_).Run(
              cast_sender_->audio_frame_input());
        }
      } else {
        if (!video_frame_input_available_callback_.is_null()) {
          base::ResetAndReturn(&video_frame_input_available_callback_).Run(
              cast_sender_->video_frame_input());
        }
      }
      break;
    case media::cast::STATUS_INVALID_CONFIGURATION:
      error_callback.Run(base::StringPrintf("Invalid %s configuration.",
                                            is_for_audio ? "audio" : "video"));
      break;
    case media::cast::STATUS_UNSUPPORTED_CODEC:
      error_callback.Run(base::StringPrintf("%s codec not supported.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
    case media::cast::STATUS_CODEC_INIT_FAILED:
      error_callback.Run(base::StringPrintf("%s codec initialization failed.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
    case media::cast::STATUS_CODEC_RUNTIME_ERROR:
      error_callback.Run(base::StringPrintf("%s codec runtime error.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
  }
}

void CastSessionDelegate::ReceivePacket(
    scoped_ptr<media::cast::Packet> packet) {
  // Do nothing (frees packet)
}
