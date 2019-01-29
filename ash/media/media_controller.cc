// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_controller.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace ash {

namespace {

bool IsMediaSessionActionEligibleForKeyControl(
    media_session::mojom::MediaSessionAction action) {
  return action == media_session::mojom::MediaSessionAction::kPlay ||
         action == media_session::mojom::MediaSessionAction::kPause ||
         action == media_session::mojom::MediaSessionAction::kPreviousTrack ||
         action == media_session::mojom::MediaSessionAction::kNextTrack;
}

}  // namespace

MediaController::MediaController(service_manager::Connector* connector)
    : connector_(connector) {
  // If media session media key handling is enabled this will setup a connection
  // and bind an observer to the media session service.
  if (base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling))
    GetMediaSessionController();
}

MediaController::~MediaController() = default;

void MediaController::BindRequest(mojom::MediaControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MediaController::AddObserver(MediaCaptureObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaController::RemoveObserver(MediaCaptureObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaController::SetClient(mojom::MediaClientAssociatedPtrInfo client) {
  client_.Bind(std::move(client));
}

void MediaController::NotifyCaptureState(
    const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states) {
  for (auto& observer : observers_)
    observer.OnMediaCaptureChanged(capture_states);
}

void MediaController::HandleMediaPlayPause() {
  // If media session media key handling is enabled. Toggle play pause using the
  // media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->ToggleSuspendResume();
    return;
  }

  if (client_)
    client_->HandleMediaPlayPause();
}

void MediaController::HandleMediaNextTrack() {
  // If media session media key handling is enabled. Fire next track using the
  // media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->NextTrack();
    return;
  }

  if (client_)
    client_->HandleMediaNextTrack();
}

void MediaController::HandleMediaPrevTrack() {
  // If media session media key handling is enabled. Fire previous track using
  // the media session service.
  if (ShouldUseMediaSession()) {
    GetMediaSessionController()->PreviousTrack();
    return;
  }

  if (client_)
    client_->HandleMediaPrevTrack();
}

void MediaController::RequestCaptureState() {
  if (client_)
    client_->RequestCaptureState();
}

void MediaController::SuspendMediaSessions() {
  if (client_)
    client_->SuspendMediaSessions();
}

void MediaController::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  supported_media_session_action_ = false;

  for (auto action : actions) {
    if (IsMediaSessionActionEligibleForKeyControl(action)) {
      supported_media_session_action_ = true;
      return;
    }
  }
}

void MediaController::SetMediaSessionControllerForTest(
    media_session::mojom::MediaControllerPtr controller) {
  media_session_controller_ptr_ = std::move(controller);
  BindMediaControllerObserver();
}

void MediaController::FlushForTesting() {
  client_.FlushForTesting();
  media_session_controller_ptr_.FlushForTesting();
}

media_session::mojom::MediaController*
MediaController::GetMediaSessionController() {
  // |connector_| can be null in tests.
  if (connector_ && !media_session_controller_ptr_.is_bound()) {
    media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
    connector_->BindInterface(media_session::mojom::kServiceName,
                              &controller_manager_ptr);
    controller_manager_ptr->CreateActiveMediaController(
        mojo::MakeRequest(&media_session_controller_ptr_));

    media_session_controller_ptr_.set_connection_error_handler(
        base::BindRepeating(&MediaController::OnMediaSessionControllerError,
                            base::Unretained(this)));

    BindMediaControllerObserver();
  }

  return media_session_controller_ptr_.get();
}

void MediaController::OnMediaSessionControllerError() {
  media_session_controller_ptr_.reset();
  supported_media_session_action_ = false;
}

void MediaController::BindMediaControllerObserver() {
  if (!media_session_controller_ptr_.is_bound())
    return;

  media_session::mojom::MediaControllerObserverPtr observer;
  media_controller_observer_binding_.Bind(mojo::MakeRequest(&observer));
  media_session_controller_ptr_->AddObserver(std::move(observer));
}

bool MediaController::ShouldUseMediaSession() {
  return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling) &&
         GetMediaSessionController() && supported_media_session_action_;
}

}  // namespace ash
