// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_sender_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"

namespace media {
namespace cast {

// The LocalVideoFrameInput class posts all incoming video frames to the main
// cast thread for processing.
class LocalVideoFrameInput : public VideoFrameInput {
 public:
  LocalVideoFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<VideoSender> video_sender)
      : cast_environment_(cast_environment), video_sender_(video_sender) {}

  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoSender::InsertRawVideoFrame,
                                           video_sender_,
                                           video_frame,
                                           capture_time));
  }

 protected:
  virtual ~LocalVideoFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalVideoFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<VideoSender> video_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoFrameInput);
};

// The LocalAudioFrameInput class posts all incoming audio frames to the main
// cast thread for processing. Therefore frames can be inserted from any thread.
class LocalAudioFrameInput : public AudioFrameInput {
 public:
  LocalAudioFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<AudioSender> audio_sender)
      : cast_environment_(cast_environment), audio_sender_(audio_sender) {}

  virtual void InsertAudio(scoped_ptr<AudioBus> audio_bus,
                           const base::TimeTicks& recorded_time) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::InsertAudio,
                                           audio_sender_,
                                           base::Passed(&audio_bus),
                                           recorded_time));
  }

 protected:
  virtual ~LocalAudioFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalAudioFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalAudioFrameInput);
};

scoped_ptr<CastSender> CastSender::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    CastTransportSender* const transport_sender) {
  CHECK(cast_environment.get());
  return scoped_ptr<CastSender>(
      new CastSenderImpl(cast_environment, transport_sender));
}

CastSenderImpl::CastSenderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    CastTransportSender* const transport_sender)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      weak_factory_(this) {
  CHECK(cast_environment.get());
}

void CastSenderImpl::InitializeAudio(
    const AudioSenderConfig& audio_config,
    const CastInitializationCallback& cast_initialization_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(audio_config.use_external_encoder ||
        cast_environment_->HasAudioThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeAudio()";

  audio_sender_.reset(
      new AudioSender(cast_environment_, audio_config, transport_sender_));

  const CastInitializationStatus status = audio_sender_->InitializationResult();
  if (status == STATUS_AUDIO_INITIALIZED) {
    audio_frame_input_ =
        new LocalAudioFrameInput(cast_environment_, audio_sender_->AsWeakPtr());
  }
  cast_initialization_cb.Run(status);
  if (video_sender_) {
    DCHECK(audio_sender_->GetTargetPlayoutDelay() ==
           video_sender_->GetTargetPlayoutDelay());
  }
}

void CastSenderImpl::InitializeVideo(
    const VideoSenderConfig& video_config,
    const CastInitializationCallback& cast_initialization_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(video_config.use_external_encoder ||
        cast_environment_->HasVideoThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeVideo()";

  video_sender_.reset(new VideoSender(
      cast_environment_,
      video_config,
      base::Bind(&CastSenderImpl::OnVideoInitialized,
                 weak_factory_.GetWeakPtr(), cast_initialization_cb),
      create_vea_cb,
      create_video_encode_mem_cb,
      transport_sender_,
      base::Bind(&CastSenderImpl::SetTargetPlayoutDelay,
                 weak_factory_.GetWeakPtr())));
  if (audio_sender_) {
    DCHECK(audio_sender_->GetTargetPlayoutDelay() ==
           video_sender_->GetTargetPlayoutDelay());
  }
}

CastSenderImpl::~CastSenderImpl() {
  VLOG(1) << "CastSenderImpl@" << this << "::~CastSenderImpl()";
}

scoped_refptr<AudioFrameInput> CastSenderImpl::audio_frame_input() {
  return audio_frame_input_;
}

scoped_refptr<VideoFrameInput> CastSenderImpl::video_frame_input() {
  return video_frame_input_;
}

void CastSenderImpl::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  VLOG(1) << "CastSenderImpl@" << this << "::SetTargetPlayoutDelay("
          << new_target_playout_delay.InMilliseconds() << " ms)";
  if (audio_sender_) {
    audio_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
  }
  if (video_sender_) {
    video_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
  }
}

void CastSenderImpl::OnVideoInitialized(
    const CastInitializationCallback& initialization_cb,
    media::cast::CastInitializationStatus result) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  video_frame_input_ =
      new LocalVideoFrameInput(cast_environment_, video_sender_->AsWeakPtr());
  initialization_cb.Run(result);
}

}  // namespace cast
}  // namespace media
