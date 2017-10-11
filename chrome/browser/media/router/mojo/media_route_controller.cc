// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_controller.h"

#include <utility>

#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"

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

MediaRouteController::MediaRouteController(const MediaRoute::Id& route_id,
                                           content::BrowserContext* context)
    : route_id_(route_id),
      request_manager_(
          EventPageRequestManagerFactory::GetApiForBrowserContext(context)),
      media_router_(MediaRouterFactory::GetApiForBrowserContext(context)),
      binding_(this) {
  DCHECK(media_router_);
  DCHECK(request_manager_);
}

MediaRouteController::InitMojoResult
MediaRouteController::InitMojoInterfaces() {
  DCHECK(is_valid_);
  DCHECK(!mojo_media_controller_);
  DCHECK(!binding_.is_bound());

  auto request = mojo::MakeRequest(&mojo_media_controller_);
  mojo_media_controller_.set_connection_error_handler(base::BindOnce(
      &MediaRouteController::OnMojoConnectionError, base::Unretained(this)));

  mojom::MediaStatusObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));
  binding_.set_connection_error_handler(base::BindOnce(
      &MediaRouteController::OnMojoConnectionError, base::Unretained(this)));

  InitAdditionalMojoConnections();
  return std::make_pair(std::move(request), std::move(observer));
}

RouteControllerType MediaRouteController::GetType() const {
  return RouteControllerType::kGeneric;
}

void MediaRouteController::Play() {
  if (request_manager_->mojo_connections_ready()) {
    mojo_media_controller_->Play();
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouteController::Play, AsWeakPtr()),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void MediaRouteController::Pause() {
  if (request_manager_->mojo_connections_ready()) {
    mojo_media_controller_->Pause();
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouteController::Pause, AsWeakPtr()),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void MediaRouteController::Seek(base::TimeDelta time) {
  if (request_manager_->mojo_connections_ready()) {
    mojo_media_controller_->Seek(time);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouteController::Seek, AsWeakPtr(), time),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void MediaRouteController::SetMute(bool mute) {
  if (request_manager_->mojo_connections_ready()) {
    mojo_media_controller_->SetMute(mute);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouteController::SetMute, AsWeakPtr(), mute),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void MediaRouteController::SetVolume(float volume) {
  if (request_manager_->mojo_connections_ready()) {
    mojo_media_controller_->SetVolume(volume);
    return;
  }
  request_manager_->RunOrDefer(
      base::BindOnce(&MediaRouteController::SetVolume, AsWeakPtr(), volume),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void MediaRouteController::OnMediaStatusUpdated(const MediaStatus& status) {
  DCHECK(is_valid_);
  current_media_status_ = status;
  for (Observer& observer : observers_)
    observer.OnMediaStatusUpdated(status);
}

void MediaRouteController::Invalidate() {
  InvalidateInternal();
  is_valid_ = false;
  binding_.Close();
  mojo_media_controller_.reset();
  for (Observer& observer : observers_)
    observer.InvalidateController();
  // |this| is deleted here!
}

MediaRouteController::~MediaRouteController() {
  if (is_valid_)
    media_router_->DetachRouteController(route_id_, this);
}

void MediaRouteController::OnMojoConnectionError() {
  binding_.Close();
  mojo_media_controller_.reset();
}

void MediaRouteController::AddObserver(Observer* observer) {
  DCHECK(is_valid_);
  observers_.AddObserver(observer);
}

void MediaRouteController::RemoveObserver(Observer* observer) {
  DCHECK(is_valid_);
  observers_.RemoveObserver(observer);
}

// static
HangoutsMediaRouteController* HangoutsMediaRouteController::From(
    MediaRouteController* controller) {
  if (!controller || controller->GetType() != RouteControllerType::kHangouts)
    return nullptr;

  return static_cast<HangoutsMediaRouteController*>(controller);
}

HangoutsMediaRouteController::HangoutsMediaRouteController(
    const MediaRoute::Id& route_id,
    content::BrowserContext* context)
    : MediaRouteController(route_id, context) {}

HangoutsMediaRouteController::~HangoutsMediaRouteController() {}

RouteControllerType HangoutsMediaRouteController::GetType() const {
  return RouteControllerType::kHangouts;
}

void HangoutsMediaRouteController::SetLocalPresent(bool local_present) {
  if (request_manager()->mojo_connections_ready()) {
    DCHECK(mojo_hangouts_controller_);
    mojo_hangouts_controller_->SetLocalPresent(local_present);
    return;
  }
  request_manager()->RunOrDefer(
      base::BindOnce(&HangoutsMediaRouteController::SetLocalPresent,
                     base::AsWeakPtr(this), local_present),
      MediaRouteProviderWakeReason::ROUTE_CONTROLLER_COMMAND);
}

void HangoutsMediaRouteController::InitAdditionalMojoConnections() {
  auto request = mojo::MakeRequest(&mojo_hangouts_controller_);
  mojo_hangouts_controller_.set_connection_error_handler(
      base::BindOnce(&HangoutsMediaRouteController::OnMojoConnectionError,
                     base::Unretained(this)));
  mojo_media_controller()->ConnectHangoutsMediaRouteController(
      std::move(request));
}

void HangoutsMediaRouteController::OnMojoConnectionError() {
  mojo_hangouts_controller_.reset();
  MediaRouteController::OnMojoConnectionError();
}

void HangoutsMediaRouteController::InvalidateInternal() {
  mojo_hangouts_controller_.reset();
}

}  // namespace media_router
