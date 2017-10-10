// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_contents_observer.h"

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/associated_interface_provider.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/WebKit/public/platform/media_engagement.mojom.h"

namespace {

int ConvertScoreToPercentage(double score) {
  return round(score * 100);
}

void SendEngagementLevelToFrame(const url::Origin& origin,
                                content::RenderFrameHost* render_frame_host) {
  blink::mojom::MediaEngagementClientAssociatedPtr client;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetHasHighMediaEngagement(origin);
}

}  // namespace.

// This is the minimum size (in px) of each dimension that a media
// element has to be in order to be determined significant.
const gfx::Size MediaEngagementContentsObserver::kSignificantSize =
    gfx::Size(200, 140);

const char* const
    MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName =
        "Media.Engagement.ScoreAtPlayback";

const char* const MediaEngagementContentsObserver::
    kHistogramSignificantNotAddedFirstTimeName =
        "Media.Engagement.SignificantPlayers.PlayerNotAdded.FirstTime";

const char* const MediaEngagementContentsObserver::
    kHistogramSignificantNotAddedAfterFirstTimeName =
        "Media.Engagement.SignificantPlayers.PlayerNotAdded.AfterFirstTime";

const char* const
    MediaEngagementContentsObserver::kHistogramSignificantRemovedName =
        "Media.Engagement.SignificantPlayers.PlayerRemoved";

const int MediaEngagementContentsObserver::kMaxInsignificantPlaybackReason =
    static_cast<int>(MediaEngagementContentsObserver::
                         InsignificantPlaybackReason::kReasonMax);

const base::TimeDelta
    MediaEngagementContentsObserver::kSignificantMediaPlaybackTime =
        base::TimeDelta::FromSeconds(7);

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

  MediaEngagementScore score = service_->CreateEngagementScore(url);
  ukm::builders::Media_Engagement_SessionFinished(source_id)
      .SetPlaybacks_Total(score.media_playbacks())
      .SetVisits_Total(score.visits())
      .SetEngagement_Score(ConvertScoreToPercentage(score.actual_score()))
      .SetPlaybacks_Delta(significant_playback_recorded_)
      .SetEngagement_IsHigh(score.high_score())
      .Record(ukm_recorder);
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

MediaEngagementContentsObserver::PlayerState::PlayerState() = default;

MediaEngagementContentsObserver::PlayerState&
MediaEngagementContentsObserver::PlayerState::operator=(const PlayerState&) =
    default;

MediaEngagementContentsObserver::PlayerState&
MediaEngagementContentsObserver::GetPlayerState(const MediaPlayerId& id) {
  auto state = player_states_.find(id);
  if (state != player_states_.end())
    return state->second;

  player_states_[id] = PlayerState();
  return player_states_[id];
}

void MediaEngagementContentsObserver::MediaStartedPlaying(
    const MediaPlayerInfo& media_player_info,
    const MediaPlayerId& media_player_id) {
  PlayerState& state = GetPlayerState(media_player_id);
  state.playing = true;
  state.has_audio = media_player_info.has_audio;
  state.has_video = media_player_info.has_video;

  MaybeInsertRemoveSignificantPlayer(media_player_id);
  UpdateTimer();
  RecordEngagementScoreToHistogramAtPlayback(media_player_id);
}

void MediaEngagementContentsObserver::
    RecordEngagementScoreToHistogramAtPlayback(const MediaPlayerId& id) {
  GURL url = committed_origin_.GetURL();
  if (!service_->ShouldRecordEngagement(url))
    return;

  PlayerState& state = GetPlayerState(id);
  if (!state.playing.value_or(false) || state.muted.value_or(true) ||
      !state.has_audio.value_or(false) || !state.has_video.value_or(false) ||
      state.score_recorded)
    return;

  int percentage = round(service_->GetEngagementScore(url) * 100);
  UMA_HISTOGRAM_PERCENTAGE(
      MediaEngagementContentsObserver::kHistogramScoreAtPlaybackName,
      percentage);
  state.score_recorded = true;
}

void MediaEngagementContentsObserver::MediaMutedStatusChanged(
    const MediaPlayerId& id,
    bool muted) {
  GetPlayerState(id).muted = muted;
  MaybeInsertRemoveSignificantPlayer(id);
  UpdateTimer();
  RecordEngagementScoreToHistogramAtPlayback(id);
}

void MediaEngagementContentsObserver::MediaResized(const gfx::Size& size,
                                                   const MediaPlayerId& id) {
  GetPlayerState(id).significant_size =
      (size.width() >= kSignificantSize.width() &&
       size.height() >= kSignificantSize.height());
  MaybeInsertRemoveSignificantPlayer(id);
  UpdateTimer();
}

void MediaEngagementContentsObserver::MediaStoppedPlaying(
    const MediaPlayerInfo& media_player_info,
    const MediaPlayerId& media_player_id) {
  GetPlayerState(media_player_id).playing = false;
  MaybeInsertRemoveSignificantPlayer(media_player_id);
  UpdateTimer();
}

void MediaEngagementContentsObserver::DidUpdateAudioMutingState(bool muted) {
  UpdateTimer();
}

std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
MediaEngagementContentsObserver::GetInsignificantPlayerReasons(
    const PlayerState& state) {
  std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
      reasons;

  if (state.muted.value_or(true)) {
    reasons.push_back(MediaEngagementContentsObserver::
                          InsignificantPlaybackReason::kAudioMuted);
  }

  if (!state.playing.value_or(false)) {
    reasons.push_back(MediaEngagementContentsObserver::
                          InsignificantPlaybackReason::kMediaPaused);
  }

  if (!state.significant_size.value_or(false) &&
      state.has_video.value_or(false)) {
    reasons.push_back(MediaEngagementContentsObserver::
                          InsignificantPlaybackReason::kFrameSizeTooSmall);
  }

  if (!state.has_audio.value_or(false)) {
    reasons.push_back(MediaEngagementContentsObserver::
                          InsignificantPlaybackReason::kNoAudioTrack);
  }

  return reasons;
}

bool MediaEngagementContentsObserver::IsPlayerStateComplete(
    const PlayerState& state) {
  return state.muted.has_value() && state.playing.has_value() &&
         state.has_audio.has_value() && state.has_video.has_value() &&
         (!state.has_video.value_or(false) ||
          state.significant_size.has_value());
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

void MediaEngagementContentsObserver::RecordInsignificantReasons(
    std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
        reasons,
    const PlayerState& state,
    MediaEngagementContentsObserver::InsignificantHistogram histogram) {
  DCHECK(IsPlayerStateComplete(state));

  std::string histogram_name;
  switch (histogram) {
    case MediaEngagementContentsObserver::InsignificantHistogram::
        kPlayerRemoved:
      histogram_name =
          MediaEngagementContentsObserver::kHistogramSignificantRemovedName;
      break;
    case MediaEngagementContentsObserver::InsignificantHistogram::
        kPlayerNotAddedFirstTime:
      histogram_name = MediaEngagementContentsObserver::
          kHistogramSignificantNotAddedFirstTimeName;
      break;
    case MediaEngagementContentsObserver::InsignificantHistogram::
        kPlayerNotAddedAfterFirstTime:
      histogram_name = MediaEngagementContentsObserver::
          kHistogramSignificantNotAddedAfterFirstTimeName;
      break;
    default:
      NOTREACHED();
      break;
  }

  base::HistogramBase* base_histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1,
      MediaEngagementContentsObserver::kMaxInsignificantPlaybackReason,
      MediaEngagementContentsObserver::kMaxInsignificantPlaybackReason + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  for (auto reason : reasons)
    base_histogram->Add(static_cast<int>(reason));

  base_histogram->Add(static_cast<int>(
      MediaEngagementContentsObserver::InsignificantPlaybackReason::kCount));
}

void MediaEngagementContentsObserver::MaybeInsertRemoveSignificantPlayer(
    const MediaPlayerId& id) {
  // If we have not received the whole player state yet then we can't be
  // significant and therefore we don't want to make a decision yet.
  PlayerState& state = GetPlayerState(id);
  if (!IsPlayerStateComplete(state))
    return;

  bool is_currently_significant =
      significant_players_.find(id) != significant_players_.end();
  std::vector<MediaEngagementContentsObserver::InsignificantPlaybackReason>
      reasons = GetInsignificantPlayerReasons(state);

  if (is_currently_significant) {
    if (!reasons.empty()) {
      // We are considered significant and we have reasons why we shouldn't
      // be, so we should make the player not significant.
      significant_players_.erase(id);
      RecordInsignificantReasons(reasons, state,
                                 MediaEngagementContentsObserver::
                                     InsignificantHistogram::kPlayerRemoved);
    }
  } else {
    if (reasons.empty()) {
      // We are not considered significant but we don't have any reasons
      // why we shouldn't be. Make the player significant.
      significant_players_.insert(id);
    } else if (state.reasons_recorded) {
      RecordInsignificantReasons(
          reasons, state,
          MediaEngagementContentsObserver::InsignificantHistogram::
              kPlayerNotAddedAfterFirstTime);
    } else {
      RecordInsignificantReasons(
          reasons, state,
          MediaEngagementContentsObserver::InsignificantHistogram::
              kPlayerNotAddedFirstTime);
      state.reasons_recorded = true;
    }
  }
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
        FROM_HERE,
        MediaEngagementContentsObserver::kSignificantMediaPlaybackTime,
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

void MediaEngagementContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  // TODO(beccahughes): Convert MEI API to using origin.
  GURL url = handle->GetWebContents()->GetURL();
  if (service_->HasHighEngagement(url)) {
    SendEngagementLevelToFrame(url::Origin(handle->GetURL()),
                               handle->GetRenderFrameHost());
  }
}
