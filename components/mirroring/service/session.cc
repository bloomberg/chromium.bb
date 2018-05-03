// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include "base/logging.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/mirroring/service/udp_socket_client.h"
#include "components/mirroring/service/video_capture_client.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"

using media::cast::FrameSenderConfig;
using media::cast::RtpPayloadType;
using media::cast::CastTransportStatus;
using media::cast::FrameEvent;
using media::cast::PacketEvent;
using media::cast::OperationalStatus;
using media::cast::Packet;

namespace mirroring {

namespace {

// The interval for CastTransport to send Frame/PacketEvents to Session for
// logging.
constexpr base::TimeDelta kSendEventsInterval = base::TimeDelta::FromSeconds(1);

// The duration for OFFER/ANSWER exchange. If timeout, notify the client that
// the session failed to start.
constexpr base::TimeDelta kOfferAnswerExchangeTimeout =
    base::TimeDelta::FromSeconds(15);

class TransportClient final : public media::cast::CastTransport::Client {
 public:
  explicit TransportClient(Session* session) : session_(session) {}
  ~TransportClient() override {}

  // media::cast::CastTransport::Client implementation.

  void OnStatusChanged(CastTransportStatus status) override {
    session_->OnTransportStatusChanged(status);
  }

  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) override {
    session_->OnLoggingEventsReceived(std::move(frame_events),
                                      std::move(packet_events));
  }

  void ProcessRtpPacket(std::unique_ptr<Packet> packet) override {
    NOTREACHED();
  }

 private:
  Session* const session_;  // Outlives this class.

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

}  // namespace

Session::Session(SessionType session_type,
                 const net::IPEndPoint& receiver_endpoint,
                 SessionClient* client)
    : client_(client), weak_factory_(this) {
  DCHECK(client_);

  std::vector<FrameSenderConfig> audio_configs;
  std::vector<FrameSenderConfig> video_configs;
  if (session_type != SessionType::VIDEO_ONLY)
    audio_configs = AudioRtpStream::GetSupportedConfigs();
  if (session_type != SessionType::AUDIO_ONLY)
    video_configs = VideoRtpStream::GetSupportedConfigs(this);
  start_timeout_timer_.Start(
      FROM_HERE, kOfferAnswerExchangeTimeout,
      base::BindRepeating(&Session::OnOfferAnswerExchangeTimeout,
                          weak_factory_.GetWeakPtr()));
  client_->DoOfferAnswerExchange(
      audio_configs, video_configs,
      base::BindOnce(&Session::StartInternal, weak_factory_.GetWeakPtr(),
                     receiver_endpoint));
}

Session::~Session() {
  StopSession();
}

void Session::StartInternal(const net::IPEndPoint& receiver_endpoint,
                            const FrameSenderConfig& audio_config,
                            const FrameSenderConfig& video_config) {
  DVLOG(1) << __func__;
  start_timeout_timer_.Stop();

  DCHECK(!video_capture_client_);
  DCHECK(!cast_transport_);
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
  DCHECK(!cast_environment_);
  DCHECK(client_);

  if (audio_config.rtp_payload_type == RtpPayloadType::REMOTE_AUDIO ||
      video_config.rtp_payload_type == RtpPayloadType::REMOTE_VIDEO) {
    NOTIMPLEMENTED();  // TODO(xjz): Add support for media remoting.
    return;
  }

  const bool has_audio =
      (audio_config.rtp_payload_type < RtpPayloadType::AUDIO_LAST) &&
      (audio_config.rtp_payload_type >= RtpPayloadType::FIRST);
  const bool has_video =
      (video_config.rtp_payload_type > RtpPayloadType::AUDIO_LAST) &&
      (video_config.rtp_payload_type < RtpPayloadType::LAST);
  if (!has_audio && !has_video) {
    VLOG(1) << "Incorrect ANSWER message: No audio or Video.";
    client_->OnError(SESSION_START_ERROR);
    return;
  }

  audio_encode_thread_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  video_encode_thread_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  cast_environment_ = new media::cast::CastEnvironment(
      base::DefaultTickClock::GetInstance(),
      base::ThreadTaskRunnerHandle::Get(), audio_encode_thread_,
      video_encode_thread_);
  network::mojom::NetworkContextPtr network_context;
  client_->GetNetWorkContext(mojo::MakeRequest(&network_context));
  auto udp_client = std::make_unique<UdpSocketClient>(
      receiver_endpoint, std::move(network_context),
      base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                     SessionError::CAST_TRANSPORT_ERROR));
  cast_transport_ = media::cast::CastTransport::Create(
      cast_environment_->Clock(), kSendEventsInterval,
      std::make_unique<TransportClient>(this), std::move(udp_client),
      base::ThreadTaskRunnerHandle::Get());

  if (has_audio) {
    auto audio_sender = std::make_unique<media::cast::AudioSender>(
        cast_environment_, audio_config,
        base::BindRepeating(&Session::OnEncoderStatusChange,
                            weak_factory_.GetWeakPtr()),
        cast_transport_.get());
    audio_stream_ = std::make_unique<AudioRtpStream>(
        std::move(audio_sender), weak_factory_.GetWeakPtr());
    // TODO(xjz): Start audio capturing.
    NOTIMPLEMENTED();
  }

  if (has_video) {
    auto video_sender = std::make_unique<media::cast::VideoSender>(
        cast_environment_, video_config,
        base::BindRepeating(&Session::OnEncoderStatusChange,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&Session::CreateVideoEncodeAccelerator,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&Session::CreateVideoEncodeMemory,
                            weak_factory_.GetWeakPtr()),
        cast_transport_.get(),
        base::BindRepeating(&Session::SetTargetPlayoutDelay,
                            weak_factory_.GetWeakPtr()));
    video_stream_ = std::make_unique<VideoRtpStream>(
        std::move(video_sender), weak_factory_.GetWeakPtr());
    media::mojom::VideoCaptureHostPtr video_host;
    client_->GetVideoCaptureHost(mojo::MakeRequest(&video_host));
    video_capture_client_ =
        std::make_unique<VideoCaptureClient>(std::move(video_host));
    video_capture_client_->Start(
        base::BindRepeating(&VideoRtpStream::InsertVideoFrame,
                            video_stream_->AsWeakPtr()),
        base::BindOnce(&Session::ReportError, weak_factory_.GetWeakPtr(),
                       SessionError::VIDEO_CAPTURE_ERROR));
  }

  client_->DidStart();
}

void Session::ReportError(SessionError error) {
  DVLOG(1) << __func__ << ": error=" << error;
  if (client_)
    client_->OnError(error);
  StopSession();
}

void Session::StopSession() {
  DVLOG(1) << __func__;
  if (!client_)
    return;

  weak_factory_.InvalidateWeakPtrs();
  start_timeout_timer_.Stop();
  audio_encode_thread_ = nullptr;
  video_encode_thread_ = nullptr;
  video_capture_client_.reset();
  audio_stream_.reset();
  video_stream_.reset();
  cast_transport_.reset();
  cast_environment_ = nullptr;
  client_->DidStop();
  client_ = nullptr;
}

void Session::OnError(const std::string& message) {
  VLOG(1) << message;
  ReportError(SessionError::CAST_STREAMING_ERROR);
}

void Session::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  if (video_capture_client_)
    video_capture_client_->RequestRefreshFrame();
}

void Session::OnEncoderStatusChange(OperationalStatus status) {
  switch (status) {
    case OperationalStatus::STATUS_UNINITIALIZED:
    case OperationalStatus::STATUS_CODEC_REINIT_PENDING:
    // Not an error.
    // TODO(miu): As an optimization, signal the client to pause sending more
    // frames until the state becomes STATUS_INITIALIZED again.
    case OperationalStatus::STATUS_INITIALIZED:
      break;
    case OperationalStatus::STATUS_INVALID_CONFIGURATION:
    case OperationalStatus::STATUS_UNSUPPORTED_CODEC:
    case OperationalStatus::STATUS_CODEC_INIT_FAILED:
    case OperationalStatus::STATUS_CODEC_RUNTIME_ERROR:
      DVLOG(1) << "OperationalStatus error.";
      ReportError(SessionError::CAST_STREAMING_ERROR);
      break;
  }
}

media::VideoEncodeAccelerator::SupportedProfiles
Session::GetSupportedVideoEncodeAcceleratorProfiles() {
  // TODO(xjz): Establish GPU channel and query for the supported profiles.
  return media::VideoEncodeAccelerator::SupportedProfiles();
}

void Session::CreateVideoEncodeAccelerator(
    const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback) {
  DVLOG(1) << __func__;
  // TODO(xjz): Establish GPU channel and create the
  // media::MojoVideoEncodeAccelerator with the gpu info.
  if (!callback.is_null())
    callback.Run(video_encode_thread_, nullptr);
}

void Session::CreateVideoEncodeMemory(
    size_t size,
    const media::cast::ReceiveVideoEncodeMemoryCallback& callback) {
  DVLOG(1) << __func__;

  mojo::ScopedSharedBufferHandle mojo_buf =
      mojo::SharedBufferHandle::Create(size);
  if (!mojo_buf->is_valid()) {
    LOG(WARNING) << "Browser failed to allocate shared memory.";
    callback.Run(nullptr);
    return;
  }

  base::SharedMemoryHandle shared_buf;
  if (mojo::UnwrapSharedMemoryHandle(std::move(mojo_buf), &shared_buf, nullptr,
                                     nullptr) != MOJO_RESULT_OK) {
    LOG(WARNING) << "Browser failed to allocate shared memory.";
    callback.Run(nullptr);
    return;
  }

  callback.Run(std::make_unique<base::SharedMemory>(shared_buf, false));
}

void Session::OnTransportStatusChanged(CastTransportStatus status) {
  DVLOG(1) << __func__ << ": status=" << status;
  switch (status) {
    case CastTransportStatus::TRANSPORT_STREAM_UNINITIALIZED:
    case CastTransportStatus::TRANSPORT_STREAM_INITIALIZED:
      return;  // Not errors, do nothing.
    case CastTransportStatus::TRANSPORT_INVALID_CRYPTO_CONFIG:
      DVLOG(1) << "Warning: unexpected status: "
               << "TRANSPORT_INVALID_CRYPTO_CONFIG";
      ReportError(SessionError::CAST_TRANSPORT_ERROR);
      break;
    case CastTransportStatus::TRANSPORT_SOCKET_ERROR:
      DVLOG(1) << "Warning: unexpected status: "
               << "TRANSPORT_SOCKET_ERROR";
      ReportError(SessionError::CAST_TRANSPORT_ERROR);
      break;
  }
}

void Session::OnLoggingEventsReceived(
    std::unique_ptr<std::vector<FrameEvent>> frame_events,
    std::unique_ptr<std::vector<PacketEvent>> packet_events) {
  DCHECK(cast_environment_);
  cast_environment_->logger()->DispatchBatchOfEvents(std::move(frame_events),
                                                     std::move(packet_events));
}

void Session::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  if (audio_stream_)
    audio_stream_->SetTargetPlayoutDelay(playout_delay);
  if (video_stream_)
    video_stream_->SetTargetPlayoutDelay(playout_delay);
}

void Session::OnOfferAnswerExchangeTimeout() {
  VLOG(1) << "OFFER/ANSWER exchange timed out.";
  DCHECK(client_);
  client_->OnError(SESSION_START_ERROR);
}

}  // namespace mirroring
