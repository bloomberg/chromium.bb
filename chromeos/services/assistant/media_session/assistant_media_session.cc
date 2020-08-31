// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "services/media_session/public/cpp/features.h"

// A macro which ensures we are running on the main thread.
#define ENSURE_MAIN_THREAD(method, ...)                                     \
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {                   \
    main_task_runner_->PostTask(                                            \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

namespace chromeos {
namespace assistant {

namespace {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;

const char kAudioFocusSourceName[] = "assistant";

}  // namespace

AssistantMediaSession::AssistantMediaSession(
    AssistantManagerService* assistant_manager_service)
    : assistant_manager_service_(assistant_manager_service),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

AssistantMediaSession::~AssistantMediaSession() {
  AbandonAudioFocusIfNeeded();
}

void AssistantMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(session_info_.Clone());
}

void AssistantMediaSession::AddObserver(
    mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer) {
  ENSURE_MAIN_THREAD(&AssistantMediaSession::AddObserver, std::move(observer));
  mojo::Remote<media_session::mojom::MediaSessionObserver>
      media_session_observer(std::move(observer));
  media_session_observer->MediaSessionInfoChanged(session_info_.Clone());
  media_session_observer->MediaSessionMetadataChanged(metadata_);
  observers_.Add(std::move(media_session_observer));
}

void AssistantMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  std::move(callback).Run(media_session::mojom::MediaSessionDebugInfo::New());
}

void AssistantMediaSession::StartDucking() {
  if (!IsSessionStateActive())
    return;
  SetAudioFocusInfo(MediaSessionInfo::SessionState::kDucking,
                    audio_focus_type_);
}

void AssistantMediaSession::StopDucking() {
  if (!IsSessionStateDucking())
    return;
  SetAudioFocusInfo(MediaSessionInfo::SessionState::kActive, audio_focus_type_);
}

void AssistantMediaSession::Suspend(SuspendType suspend_type) {
  if (!IsSessionStateActive() && !IsSessionStateDucking())
    return;
  SetAudioFocusInfo(MediaSessionInfo::SessionState::kSuspended,
                    audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPause);
}

void AssistantMediaSession::Resume(SuspendType suspend_type) {
  if (!IsSessionStateSuspended())
    return;
  SetAudioFocusInfo(MediaSessionInfo::SessionState::kActive, audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPlay);
}

void AssistantMediaSession::RequestAudioFocus(AudioFocusType audio_focus_type) {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (audio_focus_request_client_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    audio_focus_request_client_->RequestAudioFocus(
        session_info_.Clone(), audio_focus_type,
        base::BindOnce(&AssistantMediaSession::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
    return;
  }

  EnsureServiceConnection();

  // Create a mojo interface pointer to our media session.
  receiver_.reset();
  audio_focus_manager_->RequestAudioFocus(
      audio_focus_request_client_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote(), session_info_.Clone(),
      audio_focus_type,
      base::BindOnce(&AssistantMediaSession::FinishInitialAudioFocusRequest,
                     base::Unretained(this), audio_focus_type));
}

void AssistantMediaSession::AbandonAudioFocusIfNeeded() {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (IsSessionStateInactive())
    return;

  SetAudioFocusInfo(MediaSessionInfo::SessionState::kInactive,
                    audio_focus_type_);

  if (!audio_focus_request_client_.is_bound())
    return;

  audio_focus_request_client_->AbandonAudioFocus();
  audio_focus_request_client_.reset();
  audio_focus_manager_.reset();
  internal_audio_focus_id_ = base::UnguessableToken::Null();
}

void AssistantMediaSession::NotifyMediaSessionMetadataChanged(
    const assistant_client::MediaStatus& status) {
  ENSURE_MAIN_THREAD(&AssistantMediaSession::NotifyMediaSessionMetadataChanged,
                     status);
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

base::WeakPtr<AssistantMediaSession> AssistantMediaSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool AssistantMediaSession::IsSessionStateActive() const {
  return session_info_.state == MediaSessionInfo::SessionState::kActive;
}

bool AssistantMediaSession::IsSessionStateDucking() const {
  return session_info_.state == MediaSessionInfo::SessionState::kDucking;
}

bool AssistantMediaSession::IsSessionStateSuspended() const {
  return session_info_.state == MediaSessionInfo::SessionState::kSuspended;
}

bool AssistantMediaSession::IsSessionStateInactive() const {
  return session_info_.state == MediaSessionInfo::SessionState::kInactive;
}

void AssistantMediaSession::EnsureServiceConnection() {
  DCHECK(base::FeatureList::IsEnabled(
      media_session::features::kMediaSessionService));

  if (audio_focus_manager_.is_bound() && audio_focus_manager_.is_connected())
    return;

  audio_focus_manager_.reset();

  AssistantClient::Get()->RequestAudioFocusManager(
      audio_focus_manager_.BindNewPipeAndPassReceiver());
  audio_focus_manager_->SetSource(base::UnguessableToken::Create(),
                                  kAudioFocusSourceName);
}

void AssistantMediaSession::FinishAudioFocusRequest(
    AudioFocusType audio_focus_type) {
  DCHECK(audio_focus_request_client_.is_bound());

  SetAudioFocusInfo(MediaSessionInfo::SessionState::kActive, audio_focus_type);
}

void AssistantMediaSession::FinishInitialAudioFocusRequest(
    AudioFocusType audio_focus_type,
    const base::UnguessableToken& request_id) {
  internal_audio_focus_id_ = request_id;
  FinishAudioFocusRequest(audio_focus_type);
}

void AssistantMediaSession::SetAudioFocusInfo(
    MediaSessionInfo::SessionState audio_focus_state,
    AudioFocusType audio_focus_type) {
  if (audio_focus_state == session_info_.state &&
      audio_focus_type == audio_focus_type_) {
    return;
  }

  // Update |session_info_| and |audio_focus_type_|.
  session_info_.state = audio_focus_state;
  audio_focus_type_ = audio_focus_type;
  session_info_.is_controllable =
      !IsSessionStateInactive() &&
      (audio_focus_type != AudioFocusType::kGainTransient);

  NotifyMediaSessionInfoChanged();
}

void AssistantMediaSession::NotifyMediaSessionInfoChanged() {
  ENSURE_MAIN_THREAD(&AssistantMediaSession::NotifyMediaSessionInfoChanged);
  if (audio_focus_request_client_.is_bound())
    audio_focus_request_client_->MediaSessionInfoChanged(session_info_.Clone());

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(session_info_.Clone());
}

}  // namespace assistant
}  // namespace chromeos
