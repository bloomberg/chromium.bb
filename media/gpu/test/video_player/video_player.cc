// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_player.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"

#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {
namespace test {

VideoPlayer::VideoPlayer()
    : video_(nullptr),
      video_player_state_(VideoPlayerState::kUninitialized),
      event_cv_(&event_lock_),
      video_player_event_counts_{},
      event_id_(0) {}

VideoPlayer::~VideoPlayer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  Destroy();
}

// static
std::unique_ptr<VideoPlayer> VideoPlayer::Create(
    FrameRenderer* frame_renderer,
    VideoFrameValidator* frame_validator) {
  auto video_player = base::WrapUnique(new VideoPlayer());
  video_player->Initialize(frame_renderer, frame_validator);
  return video_player;
}

void VideoPlayer::Initialize(FrameRenderer* frame_renderer,
                             VideoFrameValidator* frame_validator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kUninitialized);
  DCHECK(frame_renderer);
  DVLOGF(4);

  EventCallback event_cb =
      base::BindRepeating(&VideoPlayer::NotifyEvent, base::Unretained(this));

  decoder_client_ =
      VideoDecoderClient::Create(event_cb, frame_renderer, frame_validator);
  CHECK(decoder_client_) << "Failed to create decoder client";

  video_player_state_ = VideoPlayerState::kIdle;
}

void VideoPlayer::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(video_player_state_, VideoPlayerState::kDestroyed);
  DVLOGF(4);

  decoder_client_.reset();
  video_player_state_ = VideoPlayerState::kDestroyed;
}

void VideoPlayer::SetStream(const Video* const video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(video);
  DVLOGF(4);

  // Destroy the currently active decoder.
  if (video_) {
    decoder_client_->DestroyDecoder();
  }

  // Create a decoder for the specified video. We'll always use import mode as
  // this is the only mode supported by the media::VideoDecoder interface, which
  // the video decoders are being migrated to.
  VideoDecodeAccelerator::Config decoder_config(video->Profile());
  decoder_config.output_mode =
      VideoDecodeAccelerator::Config::OutputMode::IMPORT;
  decoder_client_->CreateDecoder(decoder_config, video->Data(),
                                 video->FrameChecksums());

  video_ = video;
}

void VideoPlayer::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(video_player_state_, VideoPlayerState::kIdle);
  DCHECK(video_);
  DVLOGF(4);

  // Start decoding the video.
  video_player_state_ = VideoPlayerState::kDecoding;
  decoder_client_->Play();
}

void VideoPlayer::Stop() {
  NOTIMPLEMENTED();
}

void VideoPlayer::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_client_->Reset();
}

void VideoPlayer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  decoder_client_->Flush();
}

base::TimeDelta VideoPlayer::GetCurrentTime() const {
  NOTIMPLEMENTED();
  return base::TimeDelta();
}

size_t VideoPlayer::GetCurrentFrame() const {
  NOTIMPLEMENTED();
  return 0;
}

VideoPlayerState VideoPlayer::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_state_;
}

size_t VideoPlayer::GetEventCount(VideoPlayerEvent event) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(event_lock_);
  return video_player_event_counts_[static_cast<size_t>(event)];
}

bool VideoPlayer::WaitForEvent(VideoPlayerEvent event,
                               size_t times,
                               base::TimeDelta max_wait) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(times, 1u);
  DVLOGF(4) << "Event ID: " << static_cast<size_t>(event);

  base::TimeDelta time_waiting;
  base::AutoLock auto_lock(event_lock_);
  while (true) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    event_cv_.TimedWait(max_wait);
    time_waiting += base::TimeTicks::Now() - start_time;

    // TODO(dstaessens@) Investigate whether we really need to keep the full
    // list of events for more complex testcases.
    // Go through list of events since last wait, looking for the event we're
    // interested in.
    for (; event_id_ < video_player_events_.size(); ++event_id_) {
      if (video_player_events_[event_id_] == event)
        times--;
      if (times == 0) {
        event_id_++;
        return true;
      }
    }

    // Check whether we've exceeded the maximum time we're allowed to wait.
    if (time_waiting >= max_wait)
      return false;
  }
}

void VideoPlayer::NotifyEvent(VideoPlayerEvent event) {
  base::AutoLock auto_lock(event_lock_);
  if (event == VideoPlayerEvent::kFlushDone ||
      event == VideoPlayerEvent::kResetDone) {
    video_player_state_ = VideoPlayerState::kIdle;
  }
  video_player_events_.push_back(event);
  video_player_event_counts_[static_cast<size_t>(event)]++;
  event_cv_.Signal();
}

}  // namespace test
}  // namespace media
