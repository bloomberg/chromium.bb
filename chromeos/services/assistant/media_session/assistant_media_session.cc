// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include "base/bind.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;

const char kAudioFocusSourceName[] = "assistant";

}  // namespace

AssistantMediaSession::AssistantMediaSession(
    service_manager::Connector* connector)
    : connector_(connector), binding_(this) {}

AssistantMediaSession::~AssistantMediaSession() {
  AbandonAudioFocusIfNeeded();
}

void AssistantMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoInternal());
}

void AssistantMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());
  std::move(callback).Run(std::move(info));
}

void AssistantMediaSession::RequestAudioFocus(AudioFocusType audio_focus_type) {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (request_client_ptr_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    request_client_ptr_->RequestAudioFocus(
        GetMediaSessionInfoInternal(), audio_focus_type,
        base::BindOnce(&AssistantMediaSession::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
    return;
  }

  EnsureServiceConnection();

  // Create a mojo interface pointer to our media session.
  media_session::mojom::MediaSessionPtr media_session;
  binding_.Close();
  binding_.Bind(mojo::MakeRequest(&media_session));
  audio_focus_ptr_->RequestAudioFocus(
      mojo::MakeRequest(&request_client_ptr_), std::move(media_session),
      GetMediaSessionInfoInternal(), audio_focus_type,
      base::BindOnce(&AssistantMediaSession::FinishInitialAudioFocusRequest,
                     base::Unretained(this), audio_focus_type));
}

void AssistantMediaSession::AbandonAudioFocusIfNeeded() {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (audio_focus_state_ == State::INACTIVE)
    return;

  SetAudioFocusState(State::INACTIVE);

  if (!request_client_ptr_.is_bound())
    return;

  request_client_ptr_->AbandonAudioFocus();
  request_client_ptr_.reset();
  audio_focus_ptr_.reset();
}

void AssistantMediaSession::EnsureServiceConnection() {
  DCHECK(base::FeatureList::IsEnabled(
      media_session::features::kMediaSessionService));

  if (audio_focus_ptr_.is_bound() && !audio_focus_ptr_.encountered_error())
    return;

  audio_focus_ptr_.reset();
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&audio_focus_ptr_));
  audio_focus_ptr_->SetSourceName(kAudioFocusSourceName);
}

void AssistantMediaSession::FinishAudioFocusRequest(
    AudioFocusType audio_focus_type) {
  DCHECK(request_client_ptr_.is_bound());

  SetAudioFocusState(State::ACTIVE);
}

void AssistantMediaSession::FinishInitialAudioFocusRequest(
    AudioFocusType audio_focus_type,
    const base::UnguessableToken& request_id) {
  FinishAudioFocusRequest(audio_focus_type);
}

void AssistantMediaSession::SetAudioFocusState(State audio_focus_state) {
  if (audio_focus_state == audio_focus_state_)
    return;

  audio_focus_state_ = audio_focus_state;
  NotifyMediaSessionInfoChanged();
}

media_session::mojom::MediaSessionInfoPtr
AssistantMediaSession::GetMediaSessionInfoInternal() {
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());
  // TODO(wutao): Set other states when supporting pause/resume.
  info->state = IsActive() ? MediaSessionInfo::SessionState::kActive
                           : MediaSessionInfo::SessionState::kInactive;
  return info;
}

void AssistantMediaSession::NotifyMediaSessionInfoChanged() {
  // TODO(wutao): Notify observers.
  if (request_client_ptr_.is_bound())
    request_client_ptr_->MediaSessionInfoChanged(GetMediaSessionInfoInternal());
}

bool AssistantMediaSession::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

}  // namespace assistant
}  // namespace chromeos
