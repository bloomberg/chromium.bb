// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_receiver_session_delegate.h"

#include "base/synchronization/waitable_event.h"
#include "base/values.h"

CastReceiverSessionDelegate::CastReceiverSessionDelegate()
    : weak_factory_(this) {
}
CastReceiverSessionDelegate::~CastReceiverSessionDelegate() {}

void CastReceiverSessionDelegate::LogRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  NOTREACHED();
}

void CastReceiverSessionDelegate::Start(
    const media::cast::FrameReceiverConfig& audio_config,
    const media::cast::FrameReceiverConfig& video_config,
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    scoped_ptr<base::DictionaryValue> options,
    const media::VideoCaptureFormat& format) {
  format_ = format;
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  CastSessionDelegateBase::StartUDP(local_endpoint,
                                    remote_endpoint,
                                    options.Pass());
  cast_receiver_ = media::cast::CastReceiver::Create(cast_environment_,
                                                     audio_config,
                                                     video_config,
                                                     cast_transport_.get());
  on_audio_decoded_cb_ = base::Bind(
      &CastReceiverSessionDelegate::OnDecodedAudioFrame,
      weak_factory_.GetWeakPtr());
  on_video_decoded_cb_ = base::Bind(
      &CastReceiverSessionDelegate::OnDecodedVideoFrame,
      weak_factory_.GetWeakPtr());
}

void CastReceiverSessionDelegate::ReceivePacket(
    scoped_ptr<media::cast::Packet> packet) {
  cast_receiver_->ReceivePacket(packet.Pass());
}

void CastReceiverSessionDelegate::StartAudio(
    scoped_refptr<CastReceiverAudioValve> audio_valve) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  audio_valve_ = audio_valve;
  cast_receiver_->RequestDecodedAudioFrame(on_audio_decoded_cb_);
}

void CastReceiverSessionDelegate::OnDecodedAudioFrame(
    scoped_ptr<media::AudioBus> audio_bus,
    const base::TimeTicks& playout_time,
    bool is_continous) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (!audio_valve_)
    return;

  // We're on the IO thread, which doesn't allow blocking
  // operations. Since we don't know what the Capture callback
  // will do exactly, we need to jump to a different thread.
  // Let's re-use the audio decoder thread.
  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  cast_environment_->PostTask(
      media::cast::CastEnvironment::AUDIO,
      FROM_HERE,
      base::Bind(&CastReceiverAudioValve::Capture,
                 audio_valve_,
                 base::Owned(audio_bus.release()),
                 (playout_time - now).InMilliseconds(),
                 1.0,
                 false));
  cast_receiver_->RequestDecodedAudioFrame(on_audio_decoded_cb_);
}

void CastReceiverSessionDelegate::StartVideo(
    content::VideoCaptureDeliverFrameCB video_callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  frame_callback_ = video_callback;
  cast_receiver_->RequestDecodedVideoFrame(on_video_decoded_cb_);
}

void  CastReceiverSessionDelegate::StopVideo() {
  frame_callback_ = content::VideoCaptureDeliverFrameCB();
}

void CastReceiverSessionDelegate::OnDecodedVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& playout_time,
    bool is_continous) {
  if (frame_callback_.is_null())
    return;
  frame_callback_.Run(video_frame, playout_time);
  cast_receiver_->RequestDecodedVideoFrame(on_video_decoded_cb_);
}
