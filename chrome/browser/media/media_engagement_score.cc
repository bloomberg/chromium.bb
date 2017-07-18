// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

const char MediaEngagementScore::kVisitsKey[] = "visits";
const char MediaEngagementScore::kMediaPlaybacksKey[] = "mediaPlaybacks";
const char MediaEngagementScore::kLastMediaPlaybackTimeKey[] =
    "lastMediaPlaybackTime";

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

  double internal_time;
  if (score_dict_->GetDouble(kLastMediaPlaybackTimeKey, &internal_time))
    last_media_playback_time_ = base::Time::FromInternalValue(internal_time);
}

media::mojom::MediaEngagementScoreDetails
MediaEngagementScore::GetScoreDetails() const {
  return media::mojom::MediaEngagementScoreDetails(origin_, GetTotalScore(),
                                                   visits(), media_playbacks(),
                                                   last_media_playback_time());
}

MediaEngagementScore::~MediaEngagementScore() = default;

MediaEngagementScore::MediaEngagementScore(MediaEngagementScore&&) = default;
MediaEngagementScore& MediaEngagementScore::operator=(MediaEngagementScore&&) =
    default;

double MediaEngagementScore::GetTotalScore() const {
  if (visits() < kScoreMinVisits)
    return 0;
  return static_cast<double>(media_playbacks_) / static_cast<double>(visits_);
}

void MediaEngagementScore::Commit() {
  DCHECK(settings_map_);
  if (!UpdateScoreDict())
    return;

  settings_map_->SetWebsiteSettingDefaultScope(
      origin_, GURL(), CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
      content_settings::ResourceIdentifier(), std::move(score_dict_));
}

void MediaEngagementScore::IncrementMediaPlaybacks() {
  media_playbacks_++;
  last_media_playback_time_ = clock_->Now();
}

bool MediaEngagementScore::UpdateScoreDict() {
  int stored_visits = 0;
  int stored_media_playbacks = 0;
  double stored_last_media_playback_internal = 0;

  score_dict_->GetInteger(kVisitsKey, &stored_visits);
  score_dict_->GetInteger(kMediaPlaybacksKey, &stored_media_playbacks);
  score_dict_->GetDouble(kLastMediaPlaybackTimeKey,
                         &stored_last_media_playback_internal);

  bool changed = stored_visits != visits() ||
                 stored_media_playbacks != media_playbacks() ||
                 stored_last_media_playback_internal !=
                     last_media_playback_time_.ToInternalValue();
  if (!changed)
    return false;

  score_dict_->SetInteger(kVisitsKey, visits_);
  score_dict_->SetInteger(kMediaPlaybacksKey, media_playbacks_);
  score_dict_->SetDouble(kLastMediaPlaybackTimeKey,
                         last_media_playback_time_.ToInternalValue());

  return true;
}
