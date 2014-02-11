// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
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

namespace {

// This is a dummy class that does nothing. This is needed temporarily
// to enable tests for cast.streaming extension APIs.
// The real implementation of CastTransportSender is to use IPC to send
// data to the browser process.
// See crbug.com/327482 for more details.
class DummyTransport : public media::cast::transport::CastTransportSender {
 public:
  DummyTransport() {}
  virtual ~DummyTransport() {}

  // CastTransportSender implementations.
  virtual void SetPacketReceiver(
      const media::cast::transport::PacketReceiverCallback& packet_receiver)
      OVERRIDE {}
  virtual void InsertCodedAudioFrame(
      const media::cast::transport::EncodedAudioFrame* audio_frame,
      const base::TimeTicks& recorded_time) OVERRIDE {}
  virtual void InsertCodedVideoFrame(
      const media::cast::transport::EncodedVideoFrame* video_frame,
      const base::TimeTicks& capture_time) OVERRIDE {}
  virtual void SendRtcpFromRtpSender(
      uint32 packet_type_flags,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      const media::cast::transport::RtcpDlrrReportBlock& dlrr,
      const media::cast::transport::RtcpSenderLogMessage& sender_log,
      uint32 sending_ssrc,
      const std::string& c_name) OVERRIDE {}
  virtual void ResendPackets(
      bool is_audio,
      const media::cast::transport::MissingFramesAndPacketsMap& missing_packets)
      OVERRIDE {}
  virtual void SubscribeAudioRtpStatsCallback(
      const media::cast::transport::CastTransportRtpStatistics& callback)
      OVERRIDE {}
  virtual void SubscribeVideoRtpStatsCallback(
      const media::cast::transport::CastTransportRtpStatistics& callback)
      OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyTransport);
};
}  // namespace

CastSessionDelegate::CastSessionDelegate()
    : audio_encode_thread_("CastAudioEncodeThread"),
      video_encode_thread_("CastVideoEncodeThread"),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()) {
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
}

void CastSessionDelegate::Initialize() {
  if (cast_environment_)
    return;  // Already initialized.

  cast_transport_.reset(new DummyTransport());
  audio_encode_thread_.Start();
  video_encode_thread_.Start();

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // There's no need to decode so no thread assigned for decoding.
  // Get default logging: All disabled.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::MessageLoopProxy::current(),
      audio_encode_thread_.message_loop_proxy(),
      NULL,
      video_encode_thread_.message_loop_proxy(),
      NULL,
      base::MessageLoopProxy::current(),
      media::cast::GetDefaultCastSenderLoggingConfig());
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  audio_config_.reset(new AudioSenderConfig(config));
  StartSendingInternal(callback, true);
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  video_config_.reset(new VideoSenderConfig(config));
  StartSendingInternal(callback, false);
}

void CastSessionDelegate::StartSendingInternal(
    const FrameInputAvailableCallback& callback,
    bool is_audio) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  Initialize();

  if (is_audio) {
    audio_frame_input_available_callback_.reset(
        new FrameInputAvailableCallback(callback));
  } else {
    video_frame_input_available_callback_.reset(
        new FrameInputAvailableCallback(callback));
  }

  cast_sender_.reset(CastSender::CreateCastSender(
      cast_environment_,
      audio_config_.get(),
      video_config_.get(),
      NULL,  // GPU.
      base::Bind(&CastSessionDelegate::InitializationResult,
                 base::Unretained(this)),
      cast_transport_.get()));
}

void CastSessionDelegate::InitializationResult(
    media::cast::CastInitializationStatus result) const {
  DCHECK(cast_sender_);

  // TODO(pwestin): handle the error codes.
  if (result == media::cast::STATUS_INITIALIZED) {
    if (audio_frame_input_available_callback_) {
      audio_frame_input_available_callback_->Run(cast_sender_->frame_input());
    }
    if (video_frame_input_available_callback_) {
      video_frame_input_available_callback_->Run(cast_sender_->frame_input());
    }
  }
}
