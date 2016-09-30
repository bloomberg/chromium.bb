// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session.h"

#include "content/browser/media/session/media_session_delegate.h"
#include "content/browser/media/session/media_session_observer.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "media/base/media_content_type.h"

namespace content {

namespace {

const double kDefaultVolumeMultiplier = 1.0;
const double kDuckingVolumeMultiplier = 0.2;

}  // anonymous namespace

using MediaSessionSuspendedSource =
    MediaSessionUmaHelper::MediaSessionSuspendedSource;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(MediaSession);

MediaSession::PlayerIdentifier::PlayerIdentifier(MediaSessionObserver* observer,
                                                 int player_id)
    : observer(observer),
      player_id(player_id) {
}

bool MediaSession::PlayerIdentifier::operator==(
    const PlayerIdentifier& other) const {
  return this->observer == other.observer && this->player_id == other.player_id;
}

size_t MediaSession::PlayerIdentifier::Hash::operator()(
    const PlayerIdentifier& player_identifier) const {
  size_t hash = BASE_HASH_NAMESPACE::hash<MediaSessionObserver*>()(
      player_identifier.observer);
  hash += BASE_HASH_NAMESPACE::hash<int>()(player_identifier.player_id);
  return hash;
}

// static
MediaSession* MediaSession::Get(WebContents* web_contents) {
  MediaSession* session = FromWebContents(web_contents);
  if (!session) {
    CreateForWebContents(web_contents);
    session = FromWebContents(web_contents);
    session->Initialize();
  }
  return session;
}

MediaSession::~MediaSession() {
  DCHECK(players_.empty());
  DCHECK(audio_focus_state_ == State::INACTIVE);
}

void MediaSession::WebContentsDestroyed() {
  // This should only work for tests. In production, all the players should have
  // already been removed before WebContents is destroyed.

  // TODO(zqzhang): refactor MediaSession, maybe move the interface used to talk
  // with AudioFocusManager out to a seperate class. The AudioFocusManager unit
  // tests then could mock the interface and abandon audio focus when
  // WebContents is destroyed. See https://crbug.com/651069
  players_.clear();
  pepper_players_.clear();
  AbandonSystemAudioFocusIfNeeded();
}

void MediaSession::SetMetadata(const base::Optional<MediaMetadata>& metadata) {
  metadata_ = metadata;
  // TODO(zqzhang): On Android, the metadata is sent though JNI everytime the
  // media session play/pause state changes. Need to find a way to seprate the
  // state change and Metadata update. See https://crbug.com/621855.
  static_cast<WebContentsImpl*>(web_contents())->OnMediaSessionStateChanged();
}

bool MediaSession::AddPlayer(MediaSessionObserver* observer,
                             int player_id,
                             media::MediaContentType media_content_type) {
  if (media_content_type == media::MediaContentType::Pepper)
    return AddPepperPlayer(observer, player_id);

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  // Determine the audio focus type required for playing the new player.
  // TODO(zqzhang): handle duckable and uncontrollable.
  // See https://crbug.com/639277.
  AudioFocusManager::AudioFocusType required_audio_focus_type;
  if (media_content_type == media::MediaContentType::Persistent) {
    required_audio_focus_type = AudioFocusManager::AudioFocusType::Gain;
  } else {
    required_audio_focus_type =
        AudioFocusManager::AudioFocusType::GainTransientMayDuck;
  }

  // If the audio focus is already granted and is of type Content, there is
  // nothing to do. If it is granted of type Transient the requested type is
  // also transient, there is also nothing to do. Otherwise, the session needs
  // to request audio focus again.
  if (audio_focus_state_ == State::ACTIVE &&
      (audio_focus_type_ == AudioFocusManager::AudioFocusType::Gain ||
       audio_focus_type_ == required_audio_focus_type)) {
    players_.insert(PlayerIdentifier(observer, player_id));
    return true;
  }

  State old_audio_focus_state = audio_focus_state_;
  RequestSystemAudioFocus(required_audio_focus_type);

  if (audio_focus_state_ != State::ACTIVE)
    return false;

  // The session should be reset if a player is starting while all players are
  // suspended.
  if (old_audio_focus_state != State::ACTIVE)
    players_.clear();

  players_.insert(PlayerIdentifier(observer, player_id));
  UpdateWebContents();

  return true;
}

void MediaSession::RemovePlayer(MediaSessionObserver* observer,
                                int player_id) {
  auto it = players_.find(PlayerIdentifier(observer, player_id));
  if (it != players_.end())
    players_.erase(it);

  it = pepper_players_.find(PlayerIdentifier(observer, player_id));
  if (it != pepper_players_.end())
    pepper_players_.erase(it);

  AbandonSystemAudioFocusIfNeeded();
}

void MediaSession::RemovePlayers(MediaSessionObserver* observer) {
  for (auto it = players_.begin(); it != players_.end(); ) {
    if (it->observer == observer)
      players_.erase(it++);
    else
      ++it;
  }

  for (auto it = pepper_players_.begin(); it != pepper_players_.end(); ) {
    if (it->observer == observer)
      pepper_players_.erase(it++);
    else
      ++it;
  }

  AbandonSystemAudioFocusIfNeeded();
}

void MediaSession::RecordSessionDuck() {
  uma_helper_.RecordSessionSuspended(
      MediaSessionSuspendedSource::SystemTransientDuck);
}

void MediaSession::OnPlayerPaused(MediaSessionObserver* observer,
                                  int player_id) {
  // If a playback is completed, BrowserMediaPlayerManager will call
  // OnPlayerPaused() after RemovePlayer(). This is a workaround.
  // Also, this method may be called when a player that is not added
  // to this session (e.g. a silent video) is paused. MediaSession
  // should ignore the paused player for this case.
  if (!players_.count(PlayerIdentifier(observer, player_id)))
    return;

  // If there is more than one observer, remove the paused one from the session.
  if (players_.size() != 1) {
    RemovePlayer(observer, player_id);
    return;
  }

  // Otherwise, suspend the session.
  DCHECK(!IsSuspended());
  OnSuspendInternal(SuspendType::CONTENT, State::SUSPENDED);
}

void MediaSession::Resume(SuspendType suspend_type) {
  DCHECK(IsReallySuspended());

  // When the resume requests comes from another source than system, audio focus
  // must be requested.
  if (suspend_type != SuspendType::SYSTEM) {
    // Request audio focus again in case we lost it because another app started
    // playing while the playback was paused.
    State audio_focus_state = RequestSystemAudioFocus(audio_focus_type_)
                                  ? State::ACTIVE
                                  : State::INACTIVE;
    SetAudioFocusState(audio_focus_state);

    if (audio_focus_state_ != State::ACTIVE)
      return;
  }

  OnResumeInternal(suspend_type);
}

void MediaSession::Suspend(SuspendType suspend_type) {
  DCHECK(!IsSuspended());

  OnSuspendInternal(suspend_type, State::SUSPENDED);
}

void MediaSession::Stop(SuspendType suspend_type) {
  DCHECK(audio_focus_state_ != State::INACTIVE);
  DCHECK(suspend_type != SuspendType::CONTENT);
  DCHECK(!HasPepper());

  // TODO(mlamouri): merge the logic between UI and SYSTEM.
  if (suspend_type == SuspendType::SYSTEM) {
    OnSuspendInternal(suspend_type, State::INACTIVE);
    return;
  }

  if (audio_focus_state_ != State::SUSPENDED)
    OnSuspendInternal(suspend_type, State::SUSPENDED);

  DCHECK(audio_focus_state_ == State::SUSPENDED);
  players_.clear();

  AbandonSystemAudioFocusIfNeeded();
}

void MediaSession::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  UpdateVolumeMultiplier();
}

void MediaSession::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  UpdateVolumeMultiplier();
}

void MediaSession::UpdateVolumeMultiplier() {
  for (const auto& it : players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());
  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());
}

double MediaSession::GetVolumeMultiplier() const {
  return is_ducking_ ? kDuckingVolumeMultiplier : kDefaultVolumeMultiplier;
}

bool MediaSession::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

bool MediaSession::IsReallySuspended() const {
  return audio_focus_state_ == State::SUSPENDED;
}

bool MediaSession::IsSuspended() const {
  // TODO(mlamouri): should be == State::SUSPENDED.
  return audio_focus_state_ != State::ACTIVE;
}

bool MediaSession::IsControllable() const {
  // Only media session having focus Gain can be controllable unless it is
  // inactive.
  return audio_focus_state_ != State::INACTIVE &&
         audio_focus_type_ == AudioFocusManager::AudioFocusType::Gain;
}

bool MediaSession::HasPepper() const {
  return !pepper_players_.empty();
}

std::unique_ptr<base::CallbackList<void(MediaSession::State)>::Subscription>
MediaSession::RegisterMediaSessionStateChangedCallbackForTest(
    const StateChangedCallback& cb) {
  return media_session_state_listeners_.Add(cb);
}

void MediaSession::SetDelegateForTests(
    std::unique_ptr<MediaSessionDelegate> delegate) {
  delegate_ = std::move(delegate);
}

bool MediaSession::IsActiveForTest() const {
  return audio_focus_state_ == State::ACTIVE;
}

MediaSessionUmaHelper* MediaSession::uma_helper_for_test() {
  return &uma_helper_;
}

void MediaSession::RemoveAllPlayersForTest() {
  players_.clear();
  AbandonSystemAudioFocusIfNeeded();
}

void MediaSession::OnSuspendInternal(SuspendType suspend_type,
                                     State new_state) {
  DCHECK(!HasPepper());

  DCHECK(new_state == State::SUSPENDED || new_state == State::INACTIVE);
  // UI suspend cannot use State::INACTIVE.
  DCHECK(suspend_type == SuspendType::SYSTEM || new_state == State::SUSPENDED);

  if (audio_focus_state_ != State::ACTIVE)
    return;

  switch (suspend_type) {
    case SuspendType::UI:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::UI);
      break;
    case SuspendType::SYSTEM:
      switch (new_state) {
        case State::SUSPENDED:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemTransient);
          break;
        case State::INACTIVE:
          uma_helper_.RecordSessionSuspended(
              MediaSessionSuspendedSource::SystemPermanent);
          break;
        case State::ACTIVE:
          NOTREACHED();
          break;
      }
      break;
    case SuspendType::CONTENT:
      uma_helper_.RecordSessionSuspended(MediaSessionSuspendedSource::CONTENT);
      break;
  }

  SetAudioFocusState(new_state);
  suspend_type_ = suspend_type;

  if (suspend_type != SuspendType::CONTENT) {
    // SuspendType::CONTENT happens when the suspend action came from
    // the page in which case the player is already paused.
    // Otherwise, the players need to be paused.
    for (const auto& it : players_)
      it.observer->OnSuspend(it.player_id);
  }

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, kDuckingVolumeMultiplier);

  UpdateWebContents();
}

void MediaSession::OnResumeInternal(SuspendType suspend_type) {
  if (suspend_type == SuspendType::SYSTEM && suspend_type_ != suspend_type)
    return;

  SetAudioFocusState(State::ACTIVE);

  for (const auto& it : players_)
    it.observer->OnResume(it.player_id);

  for (const auto& it : pepper_players_)
    it.observer->OnSetVolumeMultiplier(it.player_id, GetVolumeMultiplier());

  UpdateWebContents();
}

MediaSession::MediaSession(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      audio_focus_state_(State::INACTIVE),
      audio_focus_type_(
          AudioFocusManager::AudioFocusType::GainTransientMayDuck),
      is_ducking_(false) {}

void MediaSession::Initialize() {
  delegate_ = MediaSessionDelegate::Create(this);
}

bool MediaSession::RequestSystemAudioFocus(
    AudioFocusManager::AudioFocusType audio_focus_type) {
  bool result = delegate_->RequestAudioFocus(audio_focus_type);
  uma_helper_.RecordRequestAudioFocusResult(result);

  // MediaSession must change its state & audio focus type AFTER requesting
  // audio focus.
  SetAudioFocusState(result ? State::ACTIVE : State::INACTIVE);
  audio_focus_type_ = audio_focus_type;
  return result;
}

void MediaSession::AbandonSystemAudioFocusIfNeeded() {
  if (audio_focus_state_ == State::INACTIVE || !players_.empty() ||
      !pepper_players_.empty()) {
    return;
  }
  delegate_->AbandonAudioFocus();

  SetAudioFocusState(State::INACTIVE);
  UpdateWebContents();
}

void MediaSession::UpdateWebContents() {
  media_session_state_listeners_.Notify(audio_focus_state_);
  static_cast<WebContentsImpl*>(web_contents())->OnMediaSessionStateChanged();
}

void MediaSession::SetAudioFocusState(State audio_focus_state) {
  if (audio_focus_state == audio_focus_state_)
    return;

  audio_focus_state_ = audio_focus_state;
  switch (audio_focus_state_) {
    case State::ACTIVE:
      uma_helper_.OnSessionActive();
      break;
    case State::SUSPENDED:
      uma_helper_.OnSessionSuspended();
      break;
    case State::INACTIVE:
      uma_helper_.OnSessionInactive();
      break;
  }
}

bool MediaSession::AddPepperPlayer(MediaSessionObserver* observer,
                                   int player_id) {
  bool success = RequestSystemAudioFocus(
      AudioFocusManager::AudioFocusType::Gain);
  DCHECK(success);

  pepper_players_.insert(PlayerIdentifier(observer, player_id));

  observer->OnSetVolumeMultiplier(player_id, GetVolumeMultiplier());

  return true;
}

}  // namespace content
