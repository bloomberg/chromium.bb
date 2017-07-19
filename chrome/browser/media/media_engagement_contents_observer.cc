// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

constexpr base::TimeDelta kSignificantMediaPlaybackTime =
    base::TimeDelta::FromSeconds(7);

int ConvertScoreToPercentage(double score) {
  return round(score * 100);
}

}  // namespace.

// This is the minimum size (in px) of each dimension that a media
// element has to be in order to be determined significant.
const gfx::Size MediaEngagementContentsObserver::kSignificantSize =
    gfx::Size(200, 140);

const char* MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName =
    "Media.Engagement.ScoreAtPlayback";

const char* MediaEngagementContentsObserver::kUkmEntryName =
    "Media.Engagement.SessionFinished";

const char* MediaEngagementContentsObserver::kUkmMetricPlaybacksTotalName =
    "Playbacks.Total";

const char* MediaEngagementContentsObserver::kUkmMetricVisitsTotalName =
    "Visits.Total";

const char* MediaEngagementContentsObserver::kUkmMetricEngagementScoreName =
    "Engagement.Score";

const char* MediaEngagementContentsObserver::kUkmMetricPlaybacksDeltaName =
    "Playbacks.Delta";

MediaEngagementContentsObserver::MediaEngagementContentsObserver(
    content::WebContents* web_contents,
    MediaEngagementService* service)
    : WebContentsObserver(web_contents),
      service_(service),
      playback_timer_(new base::Timer(true, false)) {}

MediaEngagementContentsObserver::~MediaEngagementContentsObserver() = default;

void MediaEngagementContentsObserver::WebContentsDestroyed() {
  playback_timer_->Stop();
  RecordUkmMetrics();
  ClearPlayerStates();
  service_->contents_observers_.erase(this);
  delete this;
}

void MediaEngagementContentsObserver::ClearPlayerStates() {
  for (auto const& p : player_states_)
    delete p.second;
  player_states_.clear();
  significant_players_.clear();
}

void MediaEngagementContentsObserver::RecordUkmMetrics() {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  GURL url = committed_origin_.GetURL();
  if (!service_->ShouldRecordEngagement(url))
    return;

  ukm::SourceId source_id = ukm_recorder->GetNewSourceID();
  ukm_recorder->UpdateSourceURL(source_id, url);

  std::unique_ptr<ukm::UkmEntryBuilder> builder = ukm_recorder->GetEntryBuilder(
      source_id, MediaEngagementContentsObserver::kUkmEntryName);

  MediaEngagementScore score = service_->CreateEngagementScore(url);
  builder->AddMetric(
      MediaEngagementContentsObserver::kUkmMetricPlaybacksTotalName,
      score.media_playbacks());
  builder->AddMetric(MediaEngagementContentsObserver::kUkmMetricVisitsTotalName,
                     score.visits());
  builder->AddMetric(
      MediaEngagementContentsObserver::kUkmMetricEngagementScoreName,
      ConvertScoreToPercentage(score.GetTotalScore()));
  builder->AddMetric(
      MediaEngagementContentsObserver::kUkmMetricPlaybacksDeltaName,
      significant_playback_recorded_);
}

void MediaEngagementContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  playback_timer_->Stop();
  ClearPlayerStates();

  url::Origin new_origin(navigation_handle->GetURL());
  if (committed_origin_.IsSameOriginWith(new_origin))
    return;

  RecordUkmMetrics();

  committed_origin_ = new_origin;
  significant_playback_recorded_ = false;
  service_->RecordVisit(committed_origin_.GetURL());
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
  PlayerState* state = GetPlayerState(media_player_id);
  state->playing = true;
  state->has_audio = media_player_info.has_audio;

  MaybeInsertSignificantPlayer(media_player_id);
  UpdateTimer();
  RecordEngagementScoreToHistogramAtPlayback(media_player_id);
}

void MediaEngagementContentsObserver::
    RecordEngagementScoreToHistogramAtPlayback(const MediaPlayerId& id) {
  GURL url = committed_origin_.GetURL();
  if (!service_->ShouldRecordEngagement(url))
    return;

  PlayerState* state = GetPlayerState(id);
  if (!state->playing || state->muted || !state->has_audio ||
      state->score_recorded)
    return;

  int percentage = round(service_->GetEngagementScore(url) * 100);
  UMA_HISTOGRAM_PERCENTAGE(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName,
      percentage);
  state->score_recorded = true;
}

void MediaEngagementContentsObserver::MediaMutedStatusChanged(
    const MediaPlayerId& id,
    bool muted) {
  GetPlayerState(id)->muted = muted;

  if (muted)
    MaybeRemoveSignificantPlayer(id);
  else
    MaybeInsertSignificantPlayer(id);

  UpdateTimer();
  RecordEngagementScoreToHistogramAtPlayback(id);
}

void MediaEngagementContentsObserver::MediaResized(const gfx::Size& size,
                                                   const MediaPlayerId& id) {
  if (size.width() >= kSignificantSize.width() &&
      size.height() >= kSignificantSize.height()) {
    GetPlayerState(id)->significant_size = true;
    MaybeInsertSignificantPlayer(id);
  } else {
    GetPlayerState(id)->significant_size = false;
    MaybeRemoveSignificantPlayer(id);
  }

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
  return !state->muted && state->playing && state->significant_size;
}

void MediaEngagementContentsObserver::OnSignificantMediaPlaybackTime() {
  DCHECK(!significant_playback_recorded_);

  // Do not record significant playback if the tab did not make
  // a sound in the last two seconds.
#if defined(OS_ANDROID)
// Skipping WasRecentlyAudible check on Android (not available).
#else
  if (!web_contents()->WasRecentlyAudible())
    return;
#endif

  significant_playback_recorded_ = true;
  service_->RecordPlayback(committed_origin_.GetURL());
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
