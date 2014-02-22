// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "content/public/renderer/p2p_socket_client.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender.h"

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

static base::LazyInstance<CastThreads> g_cast_threads =
    LAZY_INSTANCE_INITIALIZER;

CastSessionDelegate::CastSessionDelegate()
    : transport_configured_(false),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()) {
  DCHECK(io_message_loop_proxy_);
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
}

void CastSessionDelegate::Initialize() {
  if (cast_environment_)
    return;  // Already initialized.

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // There's no need to decode so no thread assigned for decoding.
  // Get default logging: All disabled.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::MessageLoopProxy::current(),
      g_cast_threads.Get().GetAudioEncodeMessageLoopProxy(),
      NULL,
      g_cast_threads.Get().GetVideoEncodeMessageLoopProxy(),
      NULL,
      base::MessageLoopProxy::current(),
      media::cast::GetDefaultCastSenderLoggingConfig());
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

void CastSessionDelegate::StartUDP(
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  transport_configured_ = true;
  local_endpoint_ = local_endpoint;
  remote_endpoint_ = remote_endpoint;
  StartSendingInternal();
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
  if (!audio_config_ && !video_config_)
    return;

  Initialize();

  media::cast::transport::CastTransportConfig config;

  // TODO(hubbe): set config.aes_key and config.aes_iv_mask.
  config.local_endpoint = local_endpoint_;
  config.receiver_endpoint = remote_endpoint_;
  if (audio_config_) {
    config.audio_ssrc = audio_config_->sender_ssrc;
    config.audio_codec = audio_config_->codec;
    config.audio_rtp_config = audio_config_->rtp_config;
    config.audio_frequency = audio_config_->frequency;
    config.audio_channels = audio_config_->channels;
  }
  if (video_config_) {
    config.video_ssrc = video_config_->sender_ssrc;
    config.video_codec = video_config_->codec;
    config.video_rtp_config = video_config_->rtp_config;
  }

  cast_transport_.reset(new CastTransportSenderIPC(
      config,
      base::Bind(&CastSessionDelegate::StatusNotificationCB,
                 base::Unretained(this))));

  cast_sender_.reset(CastSender::CreateCastSender(
      cast_environment_,
      audio_config_.get(),
      video_config_.get(),
      NULL,  // GPU.
      base::Bind(&CastSessionDelegate::InitializationResult,
                 base::Unretained(this)),
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
