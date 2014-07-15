// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/in_process_receiver.h"

#include "base/bind_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/udp_transport.h"

using media::cast::CastTransportStatus;
using media::cast::UdpTransport;

namespace media {
namespace cast {

InProcessReceiver::InProcessReceiver(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config)
    : cast_environment_(cast_environment),
      local_end_point_(local_end_point),
      remote_end_point_(remote_end_point),
      audio_config_(audio_config),
      video_config_(video_config),
      weak_factory_(this) {}

InProcessReceiver::~InProcessReceiver() {
  Stop();
}

void InProcessReceiver::Start() {
  cast_environment_->PostTask(CastEnvironment::MAIN,
                              FROM_HERE,
                              base::Bind(&InProcessReceiver::StartOnMainThread,
                                         base::Unretained(this)));
}

void InProcessReceiver::Stop() {
  base::WaitableEvent event(false, false);
  if (cast_environment_->CurrentlyOn(CastEnvironment::MAIN)) {
    StopOnMainThread(&event);
  } else {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&InProcessReceiver::StopOnMainThread,
                                           base::Unretained(this),
                                           &event));
    event.Wait();
  }
}

void InProcessReceiver::StopOnMainThread(base::WaitableEvent* event) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_.reset(NULL);
  transport_.reset(NULL);
  weak_factory_.InvalidateWeakPtrs();
  event->Signal();
}

void InProcessReceiver::UpdateCastTransportStatus(CastTransportStatus status) {
  LOG_IF(ERROR, status == media::cast::TRANSPORT_SOCKET_ERROR)
      << "Transport socket error occurred.  InProcessReceiver is likely dead.";
  VLOG(1) << "CastTransportStatus is now " << status;
}

void InProcessReceiver::StartOnMainThread() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  DCHECK(!transport_ && !cast_receiver_);
  transport_.reset(
      new UdpTransport(NULL,
                       cast_environment_->GetTaskRunner(CastEnvironment::MAIN),
                       local_end_point_,
                       remote_end_point_,
                       base::Bind(&InProcessReceiver::UpdateCastTransportStatus,
                                  base::Unretained(this))));
  cast_receiver_ = CastReceiver::Create(
      cast_environment_, audio_config_, video_config_, transport_.get());

  // TODO(hubbe): Make the cast receiver do this automatically.
  transport_->StartReceiving(cast_receiver_->packet_receiver());

  PullNextAudioFrame();
  PullNextVideoFrame();
}

void InProcessReceiver::GotAudioFrame(scoped_ptr<AudioBus> audio_frame,
                                      const base::TimeTicks& playout_time,
                                      bool is_continuous) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (audio_frame.get())
    OnAudioFrame(audio_frame.Pass(), playout_time, is_continuous);
  PullNextAudioFrame();
}

void InProcessReceiver::GotVideoFrame(
    const scoped_refptr<VideoFrame>& video_frame,
    const base::TimeTicks& playout_time,
    bool is_continuous) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (video_frame)
    OnVideoFrame(video_frame, playout_time, is_continuous);
  PullNextVideoFrame();
}

void InProcessReceiver::PullNextAudioFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->RequestDecodedAudioFrame(
      base::Bind(&InProcessReceiver::GotAudioFrame,
                 weak_factory_.GetWeakPtr()));
}

void InProcessReceiver::PullNextVideoFrame() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_receiver_->RequestDecodedVideoFrame(base::Bind(
      &InProcessReceiver::GotVideoFrame, weak_factory_.GetWeakPtr()));
}

}  // namespace cast
}  // namespace media
