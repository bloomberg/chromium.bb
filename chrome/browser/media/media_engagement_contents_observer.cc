// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include "chrome/browser/media/media_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

constexpr base::TimeDelta
    MediaEngagementContentsObserver::kSignificantMediaPlaybackTime;

MediaEngagementContentsObserver::MediaEngagementContentsObserver(
    content::WebContents* web_contents,
    MediaEngagementService* service)
    : WebContentsObserver(web_contents),
      service_(service),
      playback_timer_(new base::Timer(true, false)) {}

MediaEngagementContentsObserver::~MediaEngagementContentsObserver() = default;

void MediaEngagementContentsObserver::WebContentsDestroyed() {
  playback_timer_->Stop();
  ClearPlayerStates();
  service_->contents_observers_.erase(this);
  delete this;
}

void MediaEngagementContentsObserver::ClearPlayerStates() {
  for (auto const& p : player_states_)
    delete p.second;
  player_states_.clear();
}

void MediaEngagementContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  DCHECK(!playback_timer_->IsRunning());
  DCHECK(significant_players_.empty());

  ClearPlayerStates();

  url::Origin new_origin(navigation_handle->GetURL());
  if (committed_origin_.IsSameOriginWith(new_origin))
    return;

  committed_origin_ = new_origin;
  significant_playback_recorded_ = false;

  if (committed_origin_.unique())
    return;

  // TODO(mlamouri): record the visit into content settings.
}

void MediaEngagementContentsObserver::WasShown() {
  is_visible_ = true;
  UpdateTimer();
}

void MediaEngagementContentsObserver::WasHidden() {
  is_visible_ = false;
  UpdateTimer();
}

MediaEngagementContentsObserver::PlayerState*
MediaEngagementContentsObserver::GetPlayerState(const MediaPlayerId& id) {
  auto state = player_states_.find(id);
  if (state != player_states_.end())
    return state->second;

  PlayerState* new_state = new PlayerState();
  player_states_[id] = new_state;
  return new_state;
}

void MediaEngagementContentsObserver::MediaStartedPlaying(
    const MediaPlayerInfo& media_player_info,
    const MediaPlayerId& media_player_id) {
  // TODO(mlamouri): check if:
  // - the playback has the minimum size requirements;
  if (!media_player_info.has_audio)
    return;

  GetPlayerState(media_player_id)->playing = true;
  MaybeInsertSignificantPlayer(media_player_id);
  UpdateTimer();
}

void MediaEngagementContentsObserver::MediaMutedStateChanged(
    const MediaPlayerId& id,
    bool muted_state) {
  GetPlayerState(id)->muted = muted_state;

  if (muted_state)
    MaybeRemoveSignificantPlayer(id);
  else
    MaybeInsertSignificantPlayer(id);

  UpdateTimer();
}

void MediaEngagementContentsObserver::MediaStoppedPlaying(
    const MediaPlayerInfo& media_player_info,
    const MediaPlayerId& media_player_id) {
  GetPlayerState(media_player_id)->playing = false;
  MaybeRemoveSignificantPlayer(media_player_id);
  UpdateTimer();
}

void MediaEngagementContentsObserver::DidUpdateAudioMutingState(bool muted) {
  UpdateTimer();
}

bool MediaEngagementContentsObserver::IsSignificantPlayer(
    const MediaPlayerId& id) {
  PlayerState* state = GetPlayerState(id);
  return !state->muted && state->playing;
}

void MediaEngagementContentsObserver::OnSignificantMediaPlaybackTime() {
  DCHECK(!significant_playback_recorded_);

  significant_playback_recorded_ = true;

  if (committed_origin_.unique())
    return;

  // TODO(mlamouri): record the playback into content settings.
}

void MediaEngagementContentsObserver::MaybeInsertSignificantPlayer(
    const MediaPlayerId& id) {
  if (significant_players_.find(id) != significant_players_.end())
    return;

  if (IsSignificantPlayer(id))
    significant_players_.insert(id);
}

void MediaEngagementContentsObserver::MaybeRemoveSignificantPlayer(
    const MediaPlayerId& id) {
  if (significant_players_.find(id) == significant_players_.end())
    return;

  if (!IsSignificantPlayer(id))
    significant_players_.erase(id);
}

bool MediaEngagementContentsObserver::AreConditionsMet() const {
  if (significant_players_.empty() || !is_visible_)
    return false;

  return !web_contents()->IsAudioMuted();
}

void MediaEngagementContentsObserver::UpdateTimer() {
  if (significant_playback_recorded_)
    return;

  if (AreConditionsMet()) {
    if (playback_timer_->IsRunning())
      return;
    playback_timer_->Start(
        FROM_HERE, kSignificantMediaPlaybackTime,
        base::Bind(
            &MediaEngagementContentsObserver::OnSignificantMediaPlaybackTime,
            base::Unretained(this)));
  } else {
    if (!playback_timer_->IsRunning())
      return;
    playback_timer_->Stop();
  }
}

void MediaEngagementContentsObserver::SetTimerForTest(
    std::unique_ptr<base::Timer> timer) {
  playback_timer_ = std::move(timer);
}
