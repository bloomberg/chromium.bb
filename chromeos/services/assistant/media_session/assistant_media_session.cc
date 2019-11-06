// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "services/media_session/public/cpp/features.h"

namespace chromeos {
namespace assistant {

namespace {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;

const char kAudioFocusSourceName[] = "assistant";

}  // namespace

AssistantMediaSession::AssistantMediaSession(
    mojom::Client* client,
    AssistantManagerServiceImpl* assistant_manager)
    : assistant_manager_service_(assistant_manager),
      client_(client),
      ducking_observers_(base::MakeRefCounted<
                         base::ObserverListThreadSafe<DuckingObserver>>()) {}

AssistantMediaSession::~AssistantMediaSession() {
  AbandonAudioFocusIfNeeded();
}

void AssistantMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoInternal());
}

void AssistantMediaSession::AddObserver(
    mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer) {
  mojo::Remote<media_session::mojom::MediaSessionObserver>
      media_session_observer(std::move(observer));
  media_session_observer->MediaSessionInfoChanged(
      GetMediaSessionInfoInternal());
  media_session_observer->MediaSessionMetadataChanged(metadata_);
  observers_.Add(std::move(media_session_observer));
}

void AssistantMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());
  std::move(callback).Run(std::move(info));
}

void AssistantMediaSession::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  NotifyDucking(FROM_HERE);
}

void AssistantMediaSession::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  NotifyDucking(FROM_HERE);
}

void AssistantMediaSession::Suspend(SuspendType suspend_type) {
  if (!IsActive())
    return;

  SetAudioFocusInfo(State::SUSPENDED, audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPause);
}

void AssistantMediaSession::Resume(SuspendType suspend_type) {
  if (!IsSuspended())
    return;

  SetAudioFocusInfo(State::ACTIVE, audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPlay);
}

void AssistantMediaSession::RequestAudioFocus(AudioFocusType audio_focus_type) {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (request_client_remote_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    request_client_remote_->RequestAudioFocus(
        GetMediaSessionInfoInternal(), audio_focus_type,
        base::BindOnce(&AssistantMediaSession::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
    return;
  }

  EnsureServiceConnection();

  // Create a mojo interface pointer to our media session.
  receiver_.reset();
  audio_focus_remote_->RequestAudioFocus(
      request_client_remote_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote(), GetMediaSessionInfoInternal(),
      audio_focus_type,
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

  SetAudioFocusInfo(State::INACTIVE, audio_focus_type_);

  if (!request_client_remote_.is_bound())
    return;

  request_client_remote_->AbandonAudioFocus();
  request_client_remote_.reset();
  audio_focus_remote_.reset();
  internal_audio_focus_id_ = base::UnguessableToken::Null();
}

void AssistantMediaSession::NotifyMediaSessionMetadataChanged(
    const assistant_client::MediaStatus& status) {
  media_session::MediaMetadata metadata;

  metadata.title = base::UTF8ToUTF16(status.metadata.title);
  metadata.artist = base::UTF8ToUTF16(status.metadata.artist);
  metadata.album = base::UTF8ToUTF16(status.metadata.album);

  bool metadata_changed = metadata_ != metadata;
  if (!metadata_changed)
    return;

  metadata_ = metadata;

  current_track_ = status.track_type;

  for (auto& observer : observers_)
    observer->MediaSessionMetadataChanged(this->metadata_);
}

void AssistantMediaSession::AddDuckingObserver(DuckingObserver* observer) {
  ducking_observers_->AddObserver(observer);
  if (is_ducking_)
    observer->SetDucking(is_ducking_);
}

void AssistantMediaSession::RemoveDuckingObserver(DuckingObserver* observer) {
  ducking_observers_->RemoveObserver(observer);
}

base::WeakPtr<AssistantMediaSession> AssistantMediaSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AssistantMediaSession::EnsureServiceConnection() {
  DCHECK(base::FeatureList::IsEnabled(
      media_session::features::kMediaSessionService));

  if (audio_focus_remote_.is_bound() && audio_focus_remote_.is_connected())
    return;

  audio_focus_remote_.reset();

  client_->RequestAudioFocusManager(
      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->SetSource(base::UnguessableToken::Create(),
                                 kAudioFocusSourceName);
}

void AssistantMediaSession::FinishAudioFocusRequest(
    AudioFocusType audio_focus_type) {
  DCHECK(request_client_remote_.is_bound());

  SetAudioFocusInfo(State::ACTIVE, audio_focus_type);
}

void AssistantMediaSession::FinishInitialAudioFocusRequest(
    AudioFocusType audio_focus_type,
    const base::UnguessableToken& request_id) {
  internal_audio_focus_id_ = request_id;
  FinishAudioFocusRequest(audio_focus_type);
}

void AssistantMediaSession::SetAudioFocusInfo(State audio_focus_state,
                                              AudioFocusType audio_focus_type) {
  if (audio_focus_state == audio_focus_state_ &&
      audio_focus_type == audio_focus_type_) {
    return;
  }

  audio_focus_state_ = audio_focus_state;
  audio_focus_type_ = audio_focus_type;
  NotifyMediaSessionInfoChanged();
}

media_session::mojom::MediaSessionInfoPtr
AssistantMediaSession::GetMediaSessionInfoInternal() {
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());
  switch (audio_focus_state_) {
    case State::ACTIVE:
      info->state = MediaSessionInfo::SessionState::kActive;
      break;
    case State::SUSPENDED:
      info->state = MediaSessionInfo::SessionState::kSuspended;
      break;
    case State::INACTIVE:
      info->state = MediaSessionInfo::SessionState::kInactive;
      break;
  }
  if (audio_focus_state_ != State::INACTIVE &&
      audio_focus_type_ != AudioFocusType::kGainTransient) {
    info->is_controllable = true;
  }
  return info;
}

void AssistantMediaSession::NotifyMediaSessionInfoChanged() {
  media_session::mojom::MediaSessionInfoPtr current_info =
      GetMediaSessionInfoInternal();

  if (current_info == session_info_)
    return;

  if (request_client_remote_.is_bound())
    request_client_remote_->MediaSessionInfoChanged(current_info.Clone());

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(current_info.Clone());

  session_info_ = std::move(current_info);
}

bool AssistantMediaSession::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

bool AssistantMediaSession::IsSuspended() const {
  return audio_focus_state_ == State::SUSPENDED;
}

void AssistantMediaSession::NotifyDucking(const base::Location& location) {
  ducking_observers_->Notify(
      location, &AssistantMediaSession::DuckingObserver::SetDucking,
      is_ducking_);
}

}  // namespace assistant
}  // namespace chromeos
