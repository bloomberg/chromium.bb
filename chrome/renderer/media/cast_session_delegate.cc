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

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

CastSessionDelegate::CastSessionDelegate()
    : audio_encode_thread_("CastAudioEncodeThread"),
      video_encode_thread_("CastVideoEncodeThread"),
      audio_configured_(false),
      video_configured_(false),
      io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()) {
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
}

void CastSessionDelegate::SetSocketFactory(
    scoped_ptr<CastSession::P2PSocketFactory> socket_factory,
    const net::IPEndPoint& remote_address) {
  socket_factory_ = socket_factory.Pass();
  remote_address_ = remote_address;
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  audio_configured_ = true;
  audio_config_ = config;
  frame_input_available_callbacks_.push_back(callback);
  MaybeStartSending();
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const FrameInputAvailableCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  video_configured_ = true;
  video_config_ = config;
  frame_input_available_callbacks_.push_back(callback);
  MaybeStartSending();
}

void CastSessionDelegate::StartSending() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (cast_environment_)
    return;

  if (!socket_factory_) {
    // TODO(hubbe): Post an error back to the user
    return;
  }

  socket_ = socket_factory_->Create();
  socket_->SetDelegate(this);

  audio_encode_thread_.Start();
  video_encode_thread_.Start();

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // There's no need to decode so no thread assigned for decoding.
  // Get default logging: All disabled.
  cast_environment_ = new CastEnvironment(
      &clock_,
      base::MessageLoopProxy::current(),
      audio_encode_thread_.message_loop_proxy(),
      NULL,
      video_encode_thread_.message_loop_proxy(),
      NULL,
      media::cast::GetDefaultCastLoggingConfig());

  // TODO(hclam): Implement VideoEncoderController to configure hardware
  // encoder.
  cast_sender_.reset(CastSender::CreateCastSender(
      cast_environment_,
      audio_config_,
      video_config_,
      NULL,
      this));

  for (size_t i = 0; i < frame_input_available_callbacks_.size(); ++i) {
    frame_input_available_callbacks_[i].Run(
        cast_sender_->frame_input());
  }
  frame_input_available_callbacks_.clear();
}

  // media::cast::PacketSender Implementation
bool CastSessionDelegate::SendPacket(
    const media::cast::Packet& packet) {
  // TODO(hubbe): Make sure audio and video packets gets the right DSCP.
  socket_->SendWithDscp(
      remote_address_,
      *reinterpret_cast<const std::vector<char> *>(&packet),
      net::DSCP_AF41);
  return true;
}

bool CastSessionDelegate::SendPackets(
    const media::cast::PacketList& packets) {
  // TODO(hubbe): Add ability to send multiple packets in one IPC message.
  for (size_t i = 0; i < packets.size(); i++) {
    SendPacket(packets[i]);
  }
  return true;
}

// content::P2PSocketClient::Delegate Implementation
void CastSessionDelegate::OnOpen(
    const net::IPEndPoint& address) {
  // Called once Init completes. Ignored.
}

void CastSessionDelegate::OnIncomingTcpConnection(
    const net::IPEndPoint& address,
    content::P2PSocketClient* client) {
  // We only support UDP sockets. This function should not be called
  // for UDP sockets.
  NOTREACHED();
}

void CastSessionDelegate::OnSendComplete() {
  // Ignored for now.
}

void CastSessionDelegate::OnError() {
  // TODO(hubbe): Report this back to the user.
}

void CastSessionDelegate::OnDataReceived(const net::IPEndPoint& address,
                                         const std::vector<char>& data,
                                         const base::TimeTicks& timestamp) {
  uint8 *packet_copy = new uint8[data.size()];
  memcpy(packet_copy, &data[0], data.size());
  cast_sender_->packet_receiver()->ReceivedPacket(
      packet_copy,
      data.size(),
      base::Bind(&media::cast::PacketReceiver::DeletePacket,
                 packet_copy));
}

void CastSessionDelegate::MaybeStartSending() {
  if (!audio_configured_ || !video_configured_)
    return;
  StartSending();
}
