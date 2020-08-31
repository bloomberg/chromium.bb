// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_score.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "media/base/media_switches.h"

const char MediaEngagementScore::kVisitsKey[] = "visits";
const char MediaEngagementScore::kMediaPlaybacksKey[] = "mediaPlaybacks";
const char MediaEngagementScore::kLastMediaPlaybackTimeKey[] =
    "lastMediaPlaybackTime";
const char MediaEngagementScore::kHasHighScoreKey[] = "hasHighScore";

const char MediaEngagementScore::kScoreMinVisitsParamName[] = "min_visits";
const char MediaEngagementScore::kHighScoreLowerThresholdParamName[] =
    "lower_threshold";
const char MediaEngagementScore::kHighScoreUpperThresholdParamName[] =
    "upper_threshold";

namespace {

const int kScoreMinVisitsParamDefault = 20;
const double kHighScoreLowerThresholdParamDefault = 0.2;
const double kHighScoreUpperThresholdParamDefault = 0.3;

std::unique_ptr<base::DictionaryValue> GetMediaEngagementScoreDictForSettings(
    const HostContentSettingsMap* settings,
    const url::Origin& origin) {
  if (!settings)
    return std::make_unique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin.GetURL(), origin.GetURL(),
          ContentSettingsType::MEDIA_ENGAGEMENT,
          content_settings::ResourceIdentifier(), nullptr));

  if (value.get())
    return value;
  return std::make_unique<base::DictionaryValue>();
}

void GetIntegerFromScore(base::DictionaryValue* dict,
                         base::StringPiece key,
                         int* out) {
  if (base::Value* v = dict->FindKeyOfType(key, base::Value::Type::INTEGER))
    *out = v->GetInt();
}

}  // namespace

// static.
double MediaEngagementScore::GetHighScoreLowerThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaEngagementBypassAutoplayPolicies,
      kHighScoreLowerThresholdParamName, kHighScoreLowerThresholdParamDefault);
}

// static.
double MediaEngagementScore::GetHighScoreUpperThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      media::kMediaEngagementBypassAutoplayPolicies,
      kHighScoreUpperThresholdParamName, kHighScoreUpperThresholdParamDefault);
}

// static.
int MediaEngagementScore::GetScoreMinVisits() {
  return base::GetFieldTrialParamByFeatureAsInt(
      media::kMediaEngagementBypassAutoplayPolicies, kScoreMinVisitsParamName,
      kScoreMinVisitsParamDefault);
}

MediaEngagementScore::MediaEngagementScore(base::Clock* clock,
                                           const url::Origin& origin,
                                           HostContentSettingsMap* settings)
    : MediaEngagementScore(
          clock,
          origin,
          GetMediaEngagementScoreDictForSettings(settings, origin),
          settings) {}

MediaEngagementScore::MediaEngagementScore(
    base::Clock* clock,
    const url::Origin& origin,
    std::unique_ptr<base::DictionaryValue> score_dict,
    HostContentSettingsMap* settings)
    : origin_(origin),
      clock_(clock),
      score_dict_(score_dict.release()),
      settings_map_(settings) {
  if (!score_dict_)
    return;

  // This is to prevent using previously saved data to mark an HTTP website as
  // allowed to autoplay.
  if (base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly) &&
      origin_.scheme() != url::kHttpsScheme) {
    return;
  }

  GetIntegerFromScore(score_dict_.get(), kVisitsKey, &visits_);
  GetIntegerFromScore(score_dict_.get(), kMediaPlaybacksKey, &media_playbacks_);

  if (base::Value* value = score_dict_->FindKeyOfType(
          kHasHighScoreKey, base::Value::Type::BOOLEAN)) {
    is_high_ = value->GetBool();
  }

  if (base::Value* value = score_dict_->FindKeyOfType(
          kLastMediaPlaybackTimeKey, base::Value::Type::DOUBLE)) {
    last_media_playback_time_ =
        base::Time::FromInternalValue(value->GetDouble());
  }

  // Recalculate the total score and high bit. If the high bit changed we
  // should commit this. This should only happen if we change the threshold
  // or if we have data from before the bit was introduced.
  bool was_high = is_high_;
  Recalculate();
  bool needs_commit = is_high_ != was_high;

  // If we need to commit because of a migration and we have the settings map
  // then we should commit.
  if (needs_commit && settings_map_)
    Commit();
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

  if (origin_.opaque())
    return;

  if (!UpdateScoreDict())
    return;

  settings_map_->SetWebsiteSettingDefaultScope(
      origin_.GetURL(), GURL(), ContentSettingsType::MEDIA_ENGAGEMENT,
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

  if (!score_dict_)
    return false;

  // This is to prevent saving data that we would otherwise not use.
  if (base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly) &&
      origin_.scheme() != url::kHttpsScheme) {
    return false;
  }

  if (base::Value* value = score_dict_->FindKeyOfType(
          kHasHighScoreKey, base::Value::Type::BOOLEAN)) {
    is_high = value->GetBool();
  }

  if (base::Value* value = score_dict_->FindKeyOfType(
          kLastMediaPlaybackTimeKey, base::Value::Type::DOUBLE)) {
    stored_last_media_playback_internal = value->GetDouble();
  }

  GetIntegerFromScore(score_dict_.get(), kVisitsKey, &stored_visits);
  GetIntegerFromScore(score_dict_.get(), kMediaPlaybacksKey,
                      &stored_media_playbacks);

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

  // visitsWithMediaTag was deprecated in https://crbug.com/998687 and should
  // be removed if we see it in |score_dict_|.
  score_dict_->RemoveKey("visitsWithMediaTag");

  // These keys were deprecated in https://crbug.com/998892 and should be
  // removed if we see it in |score_dict_|.
  score_dict_->RemoveKey("audiblePlaybacks");
  score_dict_->RemoveKey("significantPlaybacks");
  score_dict_->RemoveKey("highScoreChanges");
  score_dict_->RemoveKey("mediaElementPlaybacks");
  score_dict_->RemoveKey("audioContextPlaybacks");

  return true;
}

void MediaEngagementScore::Recalculate() {
  // Use the minimum visits to compute the score to allow websites that would
  // surely have a high MEI to pass the bar early.
  double effective_visits = std::max(visits(), GetScoreMinVisits());
  actual_score_ = static_cast<double>(media_playbacks()) / effective_visits;

  // Recalculate whether the engagement score is considered high.
  if (is_high_) {
    is_high_ = actual_score_ >= GetHighScoreLowerThreshold();
  } else {
    is_high_ = actual_score_ >= GetHighScoreUpperThreshold();
  }
}

void MediaEngagementScore::SetVisits(int visits) {
  visits_ = visits;
  Recalculate();
}

void MediaEngagementScore::SetMediaPlaybacks(int media_playbacks) {
  media_playbacks_ = media_playbacks;
  Recalculate();
}
