// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include <utility>

#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

const char MediaEngagementScore::kVisitsKey[] = "visits";
const char MediaEngagementScore::kMediaPlaybacksKey[] = "mediaPlaybacks";
const char MediaEngagementScore::kLastMediaPlaybackTimeKey[] =
    "lastMediaPlaybackTime";
const char MediaEngagementScore::kHasHighScoreKey[] = "hasHighScore";

const int MediaEngagementScore::kScoreMinVisits = 5;

namespace {

std::unique_ptr<base::DictionaryValue> GetScoreDictForSettings(
    const HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return base::MakeUnique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, origin_url, CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
          content_settings::ResourceIdentifier(), nullptr));

  if (value.get())
    return value;
  return base::MakeUnique<base::DictionaryValue>();
}

}  // namespace

MediaEngagementScore::MediaEngagementScore(base::Clock* clock,
                                           const GURL& origin,
                                           HostContentSettingsMap* settings)
    : MediaEngagementScore(clock,
                           origin,
                           GetScoreDictForSettings(settings, origin)) {
  settings_map_ = settings;
}

MediaEngagementScore::MediaEngagementScore(
    base::Clock* clock,
    const GURL& origin,
    std::unique_ptr<base::DictionaryValue> score_dict)
    : origin_(origin), clock_(clock), score_dict_(score_dict.release()) {
  if (!score_dict_)
    return;

  score_dict_->GetInteger(kVisitsKey, &visits_);
  score_dict_->GetInteger(kMediaPlaybacksKey, &media_playbacks_);
  score_dict_->GetBoolean(kHasHighScoreKey, &is_high_);

  double internal_time;
  if (score_dict_->GetDouble(kLastMediaPlaybackTimeKey, &internal_time))
    last_media_playback_time_ = base::Time::FromInternalValue(internal_time);

  // Recalculate the total score and high bit. If the high bit changed we
  // should commit this. This should only happen if we change the threshold
  // or if we have data from before the bit was introduced.
  bool was_high = is_high_;
  Recalculate();
  if (is_high_ != was_high) {
    if (settings_map_) {
      Commit();
    } else {
      // Update the internal dictionary for testing.
      score_dict_->SetBoolean(kHasHighScoreKey, is_high_);
    }
  }
}

// TODO(beccahughes): Add typemap.
media::mojom::MediaEngagementScoreDetailsPtr
MediaEngagementScore::GetScoreDetails() const {
  return media::mojom::MediaEngagementScoreDetails::New(
      origin_, actual_score(), visits(), media_playbacks(),
      last_media_playback_time().ToJsTime(), high_score());
}

MediaEngagementScore::~MediaEngagementScore() = default;

MediaEngagementScore::MediaEngagementScore(MediaEngagementScore&&) = default;
MediaEngagementScore& MediaEngagementScore::operator=(MediaEngagementScore&&) =
    default;

void MediaEngagementScore::Commit() {
  DCHECK(settings_map_);
  if (!UpdateScoreDict())
    return;

  settings_map_->SetWebsiteSettingDefaultScope(
      origin_, GURL(), CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
      content_settings::ResourceIdentifier(), std::move(score_dict_));
}

void MediaEngagementScore::IncrementMediaPlaybacks() {
  SetMediaPlaybacks(media_playbacks() + 1);
  last_media_playback_time_ = clock_->Now();
}

bool MediaEngagementScore::UpdateScoreDict() {
  int stored_visits = 0;
  int stored_media_playbacks = 0;
  double stored_last_media_playback_internal = 0;
  bool is_high = false;

  score_dict_->GetInteger(kVisitsKey, &stored_visits);
  score_dict_->GetInteger(kMediaPlaybacksKey, &stored_media_playbacks);
  score_dict_->GetDouble(kLastMediaPlaybackTimeKey,
                         &stored_last_media_playback_internal);
  score_dict_->GetBoolean(kHasHighScoreKey, &is_high);

  bool changed = stored_visits != visits() ||
                 stored_media_playbacks != media_playbacks() ||
                 is_high_ != is_high ||
                 stored_last_media_playback_internal !=
                     last_media_playback_time_.ToInternalValue();
  if (!changed)
    return false;

  score_dict_->SetInteger(kVisitsKey, visits_);
  score_dict_->SetInteger(kMediaPlaybacksKey, media_playbacks_);
  score_dict_->SetDouble(kLastMediaPlaybackTimeKey,
                         last_media_playback_time_.ToInternalValue());
  score_dict_->SetBoolean(kHasHighScoreKey, is_high_);

  return true;
}

void MediaEngagementScore::Recalculate() {
  // Update the engagement score.
  actual_score_ = 0;
  if (visits() >= kScoreMinVisits) {
    actual_score_ =
        static_cast<double>(media_playbacks()) / static_cast<double>(visits());
  }

  // Recalculate whether the engagement score is considered high.
  if (is_high_)
    is_high_ = actual_score_ >= kHighScoreLowerThreshold;
  else
    is_high_ = actual_score_ >= kHighScoreUpperThreshold;
}

void MediaEngagementScore::SetVisits(int visits) {
  visits_ = visits;
  Recalculate();
}

void MediaEngagementScore::SetMediaPlaybacks(int media_playbacks) {
  media_playbacks_ = media_playbacks;
  Recalculate();
}
