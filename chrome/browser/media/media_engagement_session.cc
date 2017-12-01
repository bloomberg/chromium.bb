// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_session.h"

#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

MediaEngagementSession::MediaEngagementSession(MediaEngagementService* service,
                                               const url::Origin& origin)
    : service_(service), origin_(origin) {}

bool MediaEngagementSession::IsSameOriginWith(const url::Origin& origin) const {
  return origin_.IsSameOriginWith(origin);
}

void MediaEngagementSession::RecordSignificantPlayback() {
  DCHECK(!significant_playback_recorded_);

  significant_playback_recorded_ = true;
  pending_data_to_commit_.playback = true;

  // When playback has happened, the visit can be recorded as there will be no
  // further changes.
  CommitPendingData();
}

void MediaEngagementSession::RecordShortPlaybackIgnored(int length_msec) {
  ukm::UkmRecorder* ukm_recorder = GetUkmRecorder();
  if (!ukm_recorder)
    return;

  ukm::builders::Media_Engagement_ShortPlaybackIgnored(ukm_source_id_)
      .SetLength(length_msec)
      .Record(ukm_recorder);
}

void MediaEngagementSession::RegisterAudiblePlayers(
    int32_t audible_players,
    int32_t significant_players) {
  DCHECK_GE(audible_players, significant_players);

  if (!audible_players && !significant_players)
    return;

  pending_data_to_commit_.players = true;
  audible_players_delta_ += audible_players;
  significant_players_delta_ += significant_players;
}

bool MediaEngagementSession::significant_playback_recorded() const {
  return significant_playback_recorded_;
}

const url::Origin& MediaEngagementSession::origin() const {
  return origin_;
}

MediaEngagementSession::~MediaEngagementSession() {
  // The destructor is called when all the tabs associated te the MEI session
  // are closed. Metrics and data related to "visits" need to be recorded now.

  if (HasPendingDataToCommit())
    CommitPendingData();

  RecordUkmMetrics();
}

ukm::UkmRecorder* MediaEngagementSession::GetUkmRecorder() {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return nullptr;

  if (ukm_source_id_ == ukm::kInvalidSourceId) {
    ukm_source_id_ = ukm_recorder->GetNewSourceID();
    ukm_recorder->UpdateSourceURL(ukm_source_id_, origin_.GetURL());
  }

  return ukm_recorder;
}

void MediaEngagementSession::RecordUkmMetrics() {
  ukm::UkmRecorder* ukm_recorder = GetUkmRecorder();
  if (!ukm_recorder)
    return;

  MediaEngagementScore score =
      service_->CreateEngagementScore(origin_.GetURL());
  ukm::builders::Media_Engagement_SessionFinished(ukm_source_id_)
      .SetPlaybacks_Total(score.media_playbacks())
      .SetVisits_Total(score.visits())
      .SetEngagement_Score(round(score.actual_score() * 100))
      .SetPlaybacks_Delta(significant_playback_recorded_)
      .SetEngagement_IsHigh(score.high_score())
      .SetPlayer_Audible_Delta(audible_players_total_)
      .SetPlayer_Audible_Total(score.audible_playbacks())
      .SetPlayer_Significant_Delta(significant_players_total_)
      .SetPlayer_Significant_Total(score.significant_playbacks())
      .SetPlaybacks_SecondsSinceLast(time_since_playback_for_ukm_.InSeconds())
      .Record(ukm_recorder);
}

bool MediaEngagementSession::HasPendingDataToCommit() const {
  return pending_data_to_commit_.visit || pending_data_to_commit_.playback ||
         pending_data_to_commit_.players;
}

void MediaEngagementSession::CommitPendingData() {
  DCHECK(HasPendingDataToCommit());

  MediaEngagementScore score =
      service_->CreateEngagementScore(origin_.GetURL());

  if (pending_data_to_commit_.visit)
    score.IncrementVisits();

  if (significant_playback_recorded_ && pending_data_to_commit_.playback) {
    const base::Time old_time = score.last_media_playback_time();

    score.IncrementMediaPlaybacks();

    // This code should be reached once and |time_since_playback_for_ukm_| can't
    // be set.
    DCHECK(time_since_playback_for_ukm_.is_zero());

    if (!old_time.is_null()) {
      // Calculate the time since the last playback and the first significant
      // playback this visit. If there is no last playback time then we will
      // record 0.
      time_since_playback_for_ukm_ =
          score.last_media_playback_time() - old_time;
    }
  }

  if (pending_data_to_commit_.players) {
    score.IncrementAudiblePlaybacks(audible_players_delta_);
    score.IncrementSignificantPlaybacks(significant_players_delta_);

    audible_players_total_ += audible_players_delta_;
    significant_players_total_ += significant_players_delta_;

    audible_players_delta_ = 0;
    significant_players_delta_ = 0;
  }

  score.Commit();

  pending_data_to_commit_.visit = pending_data_to_commit_.playback =
      pending_data_to_commit_.players = false;
}
