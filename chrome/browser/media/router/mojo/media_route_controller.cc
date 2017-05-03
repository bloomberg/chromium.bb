// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <utility>

#include "chrome/browser/media/router/media_router.h"

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
  controller_ = nullptr;
  OnControllerInvalidated();
}

void MediaRouteController::Observer::OnControllerInvalidated() {}

MediaRouteController::MediaRouteController(
    const MediaRoute::Id& route_id,
    mojom::MediaControllerPtr mojo_media_controller,
    MediaRouter* media_router)
    : route_id_(route_id),
      mojo_media_controller_(std::move(mojo_media_controller)),
      media_router_(media_router),
      binding_(this) {
  DCHECK(mojo_media_controller_.is_bound());
  DCHECK(media_router);
  mojo_media_controller_.set_connection_error_handler(base::Bind(
      &MediaRouteController::OnMojoConnectionError, base::Unretained(this)));
}

void MediaRouteController::Play() {
  DCHECK(is_valid_);
  mojo_media_controller_->Play();
}

void MediaRouteController::Pause() {
  DCHECK(is_valid_);
  mojo_media_controller_->Pause();
}

void MediaRouteController::Seek(base::TimeDelta time) {
  DCHECK(is_valid_);
  mojo_media_controller_->Seek(time);
}

void MediaRouteController::SetMute(bool mute) {
  DCHECK(is_valid_);
  mojo_media_controller_->SetMute(mute);
}

void MediaRouteController::SetVolume(float volume) {
  DCHECK(is_valid_);
  mojo_media_controller_->SetVolume(volume);
}

void MediaRouteController::OnMediaStatusUpdated(const MediaStatus& status) {
  DCHECK(is_valid_);
  for (Observer& observer : observers_)
    observer.OnMediaStatusUpdated(status);
}

void MediaRouteController::Invalidate() {
  is_valid_ = false;
  binding_.Close();
  mojo_media_controller_.reset();
  for (Observer& observer : observers_)
    observer.InvalidateController();
  // |this| is deleted here!
}

mojom::MediaStatusObserverPtr MediaRouteController::BindObserverPtr() {
  DCHECK(is_valid_);
  DCHECK(!binding_.is_bound());
  mojom::MediaStatusObserverPtr observer_ptr =
      binding_.CreateInterfacePtrAndBind();
  binding_.set_connection_error_handler(base::Bind(
      &MediaRouteController::OnMojoConnectionError, base::Unretained(this)));

  return observer_ptr;
}

MediaRouteController::~MediaRouteController() {
  if (is_valid_)
    media_router_->DetachRouteController(route_id_, this);
}

void MediaRouteController::AddObserver(Observer* observer) {
  DCHECK(is_valid_);
  observers_.AddObserver(observer);
}

void MediaRouteController::RemoveObserver(Observer* observer) {
  DCHECK(is_valid_);
  observers_.RemoveObserver(observer);
}

void MediaRouteController::OnMojoConnectionError() {
  media_router_->DetachRouteController(route_id_, this);
  Invalidate();
}

}  // namespace media_router
