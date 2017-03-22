// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <utility>

namespace media_router {

MediaRouteController::Observer::Observer(
    scoped_refptr<MediaRouteController> controller)
    : controller_(std::move(controller)) {
  controller_->AddObserver(this);
}

MediaRouteController::Observer::~Observer() {
  if (controller_)
    controller_->RemoveObserver(this);
}

void MediaRouteController::Observer::InvalidateController() {
  controller_->RemoveObserver(this);
  controller_ = nullptr;
  OnControllerInvalidated();
}

void MediaRouteController::Observer::OnControllerInvalidated() {}

MediaRouteController::MediaRouteController(
    const MediaRoute::Id& route_id,
    mojom::MediaControllerPtr media_controller)
    : route_id_(route_id), media_controller_(std::move(media_controller)) {
  DCHECK(media_controller_.is_bound());
  media_controller_.set_connection_error_handler(
      base::Bind(&MediaRouteController::Invalidate, base::Unretained(this)));
}

void MediaRouteController::Play() {
  media_controller_->Play();
}

void MediaRouteController::Pause() {
  media_controller_->Pause();
}

void MediaRouteController::Seek(base::TimeDelta time) {
  media_controller_->Seek(time);
}

void MediaRouteController::SetMute(bool mute) {
  media_controller_->SetMute(mute);
}

void MediaRouteController::SetVolume(float volume) {
  media_controller_->SetVolume(volume);
}

void MediaRouteController::OnMediaStatusUpdated(const MediaStatus& status) {
  for (Observer& observer : observers_)
    observer.OnMediaStatusUpdated(status);
}

void MediaRouteController::Invalidate() {
  for (Observer& observer : observers_)
    observer.InvalidateController();
  // |this| is deleted here!
}

MediaRouteController::~MediaRouteController() {}

void MediaRouteController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaRouteController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace media_router
