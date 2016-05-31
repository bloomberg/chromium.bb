// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_score.h"

#include <cmath>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Delta within which to consider scores equal.
const double kScoreDelta = 0.001;

// Delta within which to consider internal time values equal. Internal time
// values are in microseconds, so this delta comes out at one second.
const double kTimeDelta = 1000000;

// Number of days after the last launch of an origin from an installed shortcut
// for which WEB_APP_INSTALLED_POINTS will be added to the engagement score.
const int kMaxDaysSinceShortcutLaunch = 10;

// Keys used in the variations params. Order matches
// SiteEngagementScore::Variation enum.
const char* kVariationNames[] = {
    "max_points_per_day",
    "decay_period_in_days",
    "decay_points",
    "navigation_points",
    "user_input_points",
    "visible_media_playing_points",
    "hidden_media_playing_points",
    "web_app_installed_points",
    "first_daily_engagement_points",
    "medium_engagement_boundary",
    "high_engagement_boundary",
};

bool DoublesConsideredDifferent(double value1, double value2, double delta) {
  double abs_difference = fabs(value1 - value2);
  return abs_difference > delta;
}

std::unique_ptr<base::DictionaryValue> GetScoreDictForOrigin(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return std::unique_ptr<base::DictionaryValue>();

  std::unique_ptr<base::Value> value = settings->GetWebsiteSetting(
      origin_url, origin_url, CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
      std::string(), NULL);
  if (!value.get())
    return base::WrapUnique(new base::DictionaryValue());

  if (!value->IsType(base::Value::TYPE_DICTIONARY))
    return base::WrapUnique(new base::DictionaryValue());

  return base::WrapUnique(static_cast<base::DictionaryValue*>(value.release()));
}

}  // namespace

const double SiteEngagementScore::kMaxPoints = 100;
double SiteEngagementScore::param_values[] = {
    5,     // MAX_POINTS_PER_DAY
    7,     // DECAY_PERIOD_IN_DAYS
    5,     // DECAY_POINTS
    0.5,   // NAVIGATION_POINTS
    0.2,   // USER_INPUT_POINTS
    0.02,  // VISIBLE_MEDIA_POINTS
    0.01,  // HIDDEN_MEDIA_POINTS
    5,     // WEB_APP_INSTALLED_POINTS
    0.5,   // FIRST_DAILY_ENGAGEMENT
    8,     // BOOTSTRAP_POINTS
    5,     // MEDIUM_ENGAGEMENT_BOUNDARY
    50,    // HIGH_ENGAGEMENT_BOUNDARY
};

const char* SiteEngagementScore::kRawScoreKey = "rawScore";
const char* SiteEngagementScore::kPointsAddedTodayKey = "pointsAddedToday";
const char* SiteEngagementScore::kLastEngagementTimeKey = "lastEngagementTime";
const char* SiteEngagementScore::kLastShortcutLaunchTimeKey =
    "lastShortcutLaunchTime";

double SiteEngagementScore::GetMaxPointsPerDay() {
  return param_values[MAX_POINTS_PER_DAY];
}

double SiteEngagementScore::GetDecayPeriodInDays() {
  return param_values[DECAY_PERIOD_IN_DAYS];
}

double SiteEngagementScore::GetDecayPoints() {
  return param_values[DECAY_POINTS];
}

double SiteEngagementScore::GetNavigationPoints() {
  return param_values[NAVIGATION_POINTS];
}

double SiteEngagementScore::GetUserInputPoints() {
  return param_values[USER_INPUT_POINTS];
}

double SiteEngagementScore::GetVisibleMediaPoints() {
  return param_values[VISIBLE_MEDIA_POINTS];
}

double SiteEngagementScore::GetHiddenMediaPoints() {
  return param_values[HIDDEN_MEDIA_POINTS];
}

double SiteEngagementScore::GetWebAppInstalledPoints() {
  return param_values[WEB_APP_INSTALLED_POINTS];
}

double SiteEngagementScore::GetFirstDailyEngagementPoints() {
  return param_values[FIRST_DAILY_ENGAGEMENT];
}

double SiteEngagementScore::GetBootstrapPoints() {
  return param_values[BOOTSTRAP_POINTS];
}

double SiteEngagementScore::GetMediumEngagementBoundary() {
  return param_values[MEDIUM_ENGAGEMENT_BOUNDARY];
}

double SiteEngagementScore::GetHighEngagementBoundary() {
  return param_values[HIGH_ENGAGEMENT_BOUNDARY];
}

// static
void SiteEngagementScore::UpdateFromVariations(const char* param_name) {
  double param_vals[MAX_VARIATION];

  for (int i = 0; i < MAX_VARIATION; ++i) {
    std::string param_string =
        variations::GetVariationParamValue(param_name, kVariationNames[i]);

    // Bail out if we didn't get a param string for the key, or if we couldn't
    // convert the param string to a double, or if we get a negative value.
    if (param_string.empty() ||
        !base::StringToDouble(param_string, &param_vals[i]) ||
        param_vals[i] < 0) {
      return;
    }
  }

  // Once we're sure everything is valid, assign the variation to the param
  // values array.
  for (int i = 0; i < MAX_VARIATION; ++i)
    SiteEngagementScore::param_values[i] = param_vals[i];
}

SiteEngagementScore::SiteEngagementScore(base::Clock* clock,
                                         const GURL& origin,
                                         HostContentSettingsMap* settings_map)
    : SiteEngagementScore(clock, GetScoreDictForOrigin(settings_map, origin)) {
  origin_ = origin;
  settings_map_ = settings_map;
}

SiteEngagementScore::SiteEngagementScore(SiteEngagementScore&& other) = default;

SiteEngagementScore::~SiteEngagementScore() {}

SiteEngagementScore& SiteEngagementScore::operator=(
    SiteEngagementScore&& other) = default;

void SiteEngagementScore::AddPoints(double points) {
  DCHECK_NE(0, points);
  double decayed_score = DecayedScore();

  // Record the original and decayed scores after a decay event.
  if (decayed_score < raw_score_) {
    SiteEngagementMetrics::RecordScoreDecayedFrom(raw_score_);
    SiteEngagementMetrics::RecordScoreDecayedTo(decayed_score);
  }

  // As the score is about to be updated, commit any decay that has happened
  // since the last update.
  raw_score_ = decayed_score;

  base::Time now = clock_->Now();
  if (!last_engagement_time_.is_null() &&
      now.LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    points_added_today_ = 0;
  }

  if (points_added_today_ == 0) {
    // Award bonus engagement for the first engagement of the day for a site.
    points += GetFirstDailyEngagementPoints();
    SiteEngagementMetrics::RecordEngagement(
        SiteEngagementMetrics::ENGAGEMENT_FIRST_DAILY_ENGAGEMENT);
  }

  double to_add = std::min(kMaxPoints - raw_score_,
                           GetMaxPointsPerDay() - points_added_today_);
  to_add = std::min(to_add, points);

  points_added_today_ += to_add;
  raw_score_ += to_add;

  last_engagement_time_ = now;
}

double SiteEngagementScore::GetScore() const {
  return std::min(DecayedScore() + BonusScore(), kMaxPoints);
}

void SiteEngagementScore::Commit() {
  if (!UpdateScoreDict(score_dict_.get()))
    return;

  settings_map_->SetWebsiteSettingDefaultScope(
      origin_, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      score_dict_.release());
}

bool SiteEngagementScore::MaxPointsPerDayAdded() const {
  if (!last_engagement_time_.is_null() &&
      clock_->Now().LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    return false;
  }

  return points_added_today_ == GetMaxPointsPerDay();
}

void SiteEngagementScore::Reset(double points,
                                const base::Time last_engagement_time) {
  raw_score_ = points;
  points_added_today_ = 0;

  // This must be set in order to prevent the score from decaying when read.
  last_engagement_time_ = last_engagement_time;
}

bool SiteEngagementScore::UpdateScoreDict(base::DictionaryValue* score_dict) {
  double raw_score_orig = 0;
  double points_added_today_orig = 0;
  double last_engagement_time_internal_orig = 0;
  double last_shortcut_launch_time_internal_orig = 0;

  score_dict->GetDouble(kRawScoreKey, &raw_score_orig);
  score_dict->GetDouble(kPointsAddedTodayKey, &points_added_today_orig);
  score_dict->GetDouble(kLastEngagementTimeKey,
                        &last_engagement_time_internal_orig);
  score_dict->GetDouble(kLastShortcutLaunchTimeKey,
                        &last_shortcut_launch_time_internal_orig);
  bool changed =
      DoublesConsideredDifferent(raw_score_orig, raw_score_, kScoreDelta) ||
      DoublesConsideredDifferent(points_added_today_orig, points_added_today_,
                                 kScoreDelta) ||
      DoublesConsideredDifferent(last_engagement_time_internal_orig,
                                 last_engagement_time_.ToInternalValue(),
                                 kTimeDelta) ||
      DoublesConsideredDifferent(last_shortcut_launch_time_internal_orig,
                                 last_shortcut_launch_time_.ToInternalValue(),
                                 kTimeDelta);

  if (!changed)
    return false;

  score_dict->SetDouble(kRawScoreKey, raw_score_);
  score_dict->SetDouble(kPointsAddedTodayKey, points_added_today_);
  score_dict->SetDouble(kLastEngagementTimeKey,
                        last_engagement_time_.ToInternalValue());
  score_dict->SetDouble(kLastShortcutLaunchTimeKey,
                        last_shortcut_launch_time_.ToInternalValue());

  return true;
}

SiteEngagementScore::SiteEngagementScore(
    base::Clock* clock,
    std::unique_ptr<base::DictionaryValue> score_dict)
    : clock_(clock),
      raw_score_(0),
      points_added_today_(0),
      last_engagement_time_(),
      last_shortcut_launch_time_(),
      score_dict_(score_dict.release()) {
  if (!score_dict_)
    return;

  score_dict_->GetDouble(kRawScoreKey, &raw_score_);
  score_dict_->GetDouble(kPointsAddedTodayKey, &points_added_today_);

  double internal_time;
  if (score_dict_->GetDouble(kLastEngagementTimeKey, &internal_time))
    last_engagement_time_ = base::Time::FromInternalValue(internal_time);
  if (score_dict_->GetDouble(kLastShortcutLaunchTimeKey, &internal_time))
    last_shortcut_launch_time_ = base::Time::FromInternalValue(internal_time);
}

double SiteEngagementScore::DecayedScore() const {
  // Note that users can change their clock, so from this system's perspective
  // time can go backwards. If that does happen and the system detects that the
  // current day is earlier than the last engagement, no decay (or growth) is
  // applied.
  int days_since_engagement = (clock_->Now() - last_engagement_time_).InDays();
  if (days_since_engagement < 0)
    return raw_score_;

  int periods = days_since_engagement / GetDecayPeriodInDays();
  return std::max(0.0, raw_score_ - periods * GetDecayPoints());
}

double SiteEngagementScore::BonusScore() const {
  int days_since_shortcut_launch =
      (clock_->Now() - last_shortcut_launch_time_).InDays();
  if (days_since_shortcut_launch <= kMaxDaysSinceShortcutLaunch)
    return GetWebAppInstalledPoints();

  return 0;
}

void SiteEngagementScore::SetParamValuesForTesting() {
  param_values[MAX_POINTS_PER_DAY] = 5;
  param_values[DECAY_PERIOD_IN_DAYS] = 7;
  param_values[DECAY_POINTS] = 5;
  param_values[NAVIGATION_POINTS] = 0.5;
  param_values[USER_INPUT_POINTS] = 0.05;
  param_values[VISIBLE_MEDIA_POINTS] = 0.02;
  param_values[HIDDEN_MEDIA_POINTS] = 0.01;
  param_values[WEB_APP_INSTALLED_POINTS] = 5;
  param_values[BOOTSTRAP_POINTS] = 8;
  param_values[MEDIUM_ENGAGEMENT_BOUNDARY] = 5;
  param_values[HIGH_ENGAGEMENT_BOUNDARY] = 50;

  // This is set to zero to avoid interference with tests and is set when
  // testing this functionality.
  param_values[FIRST_DAILY_ENGAGEMENT] = 0;
}
