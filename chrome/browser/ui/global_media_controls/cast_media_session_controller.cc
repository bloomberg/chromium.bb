// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"

#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/media_session/public/mojom/constants.mojom.h"

namespace {

constexpr base::TimeDelta kDefaultSeekTimeSeconds =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

bool IsPlaying(const media_router::mojom::MediaStatusPtr& media_status) {
  return media_status &&
         media_status->play_state ==
             media_router::mojom::MediaStatus::PlayState::PLAYING;
}

}  // namespace

CastMediaSessionController::CastMediaSessionController(
    mojo::Remote<media_router::mojom::MediaController> route_controller)
    : route_controller_(std::move(route_controller)) {}

CastMediaSessionController::~CastMediaSessionController() {}

void CastMediaSessionController::Send(
    media_session::mojom::MediaSessionAction action) {
  if (!media_status_)
    return;

  switch (action) {
    case media_session::mojom::MediaSessionAction::kPlay:
      route_controller_->Play();
      return;
    case media_session::mojom::MediaSessionAction::kPause:
      route_controller_->Pause();
      return;
    case media_session::mojom::MediaSessionAction::kPreviousTrack:
      route_controller_->PreviousTrack();
      return;
    case media_session::mojom::MediaSessionAction::kNextTrack:
      route_controller_->NextTrack();
      return;
    case media_session::mojom::MediaSessionAction::kSeekBackward:
      route_controller_->Seek(PutWithinBounds(media_status_->current_time -
                                              kDefaultSeekTimeSeconds));
      return;
    case media_session::mojom::MediaSessionAction::kSeekForward:
      route_controller_->Seek(PutWithinBounds(media_status_->current_time +
                                              kDefaultSeekTimeSeconds));
      return;
    case media_session::mojom::MediaSessionAction::kStop:
      route_controller_->Pause();
      return;
    case media_session::mojom::MediaSessionAction::kSkipAd:
    case media_session::mojom::MediaSessionAction::kSeekTo:
    case media_session::mojom::MediaSessionAction::kScrubTo:
    case media_session::mojom::MediaSessionAction::kEnterPictureInPicture:
    case media_session::mojom::MediaSessionAction::kExitPictureInPicture:
      NOTREACHED();
      return;
  }
}

void CastMediaSessionController::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr media_status) {
  media_status_ = std::move(media_status);
  // We start incrementing |media_status_->current_time|, so that we
  // know the approximate playback position, which is used as the baseline from
  // which we seek forward or backward. We must do this because the Cast
  // receiver only gives an update when the playback state changes (e.g. paused,
  // seeked), and not when the current position is incremented every second.
  if (IsPlaying(media_status_))
    IncrementCurrentTimeAfterOneSecond();
}

void CastMediaSessionController::FlushForTesting() {
  route_controller_.FlushForTesting();
}

base::TimeDelta CastMediaSessionController::PutWithinBounds(
    const base::TimeDelta& time) {
  if (time < base::TimeDelta() || !media_status_)
    return base::TimeDelta();
  if (time > media_status_->duration)
    return media_status_->duration;
  return time;
}

void CastMediaSessionController::IncrementCurrentTimeAfterOneSecond() {
  // Reset() cancels the previously posted callback, if it exists.
  increment_current_time_callback_.Reset(
      base::BindOnce(&CastMediaSessionController::IncrementCurrentTime,
                     weak_ptr_factory_.GetWeakPtr()));
  // TODO(crbug.com/1052156): If the playback rate is not 1, we must increment
  // at a different rate.
  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        increment_current_time_callback_.callback(),
                        base::TimeDelta::FromSeconds(1));
}

void CastMediaSessionController::IncrementCurrentTime() {
  if (!IsPlaying(media_status_))
    return;

  if (media_status_->current_time < media_status_->duration)
    IncrementCurrentTimeAfterOneSecond();
  media_status_->current_time = PutWithinBounds(
      media_status_->current_time + base::TimeDelta::FromSeconds(1));
}
