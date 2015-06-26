// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace {

// Delta within which to consider scores equal.
const double kScoreDelta = 0.001;

// Delta within which to consider internal time values equal. Internal time
// values are in microseconds, so this delta comes out at one second.
const double kTimeDelta = 1000000;

bool DoublesConsideredDifferent(double value1, double value2, double delta) {
  double abs_difference = fabs(value1 - value2);
  return abs_difference > delta;
}

scoped_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return scoped_ptr<base::DictionaryValue>();

  scoped_ptr<base::Value> value = settings->GetWebsiteSetting(
      origin_url, origin_url, CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
      std::string(), NULL);
  if (!value.get())
    return make_scoped_ptr(new base::DictionaryValue());

  if (!value->IsType(base::Value::TYPE_DICTIONARY))
    return make_scoped_ptr(new base::DictionaryValue());

  return make_scoped_ptr(static_cast<base::DictionaryValue*>(value.release()));
}

}  // namespace

const char* SiteEngagementScore::kRawScoreKey = "rawScore";
const char* SiteEngagementScore::kPointsAddedTodayKey = "pointsAddedToday";
const char* SiteEngagementScore::kLastEngagementTimeKey = "lastEngagementTime";

const double SiteEngagementScore::kMaxPoints = 100;
const double SiteEngagementScore::kMaxPointsPerDay = 5;
const double SiteEngagementScore::kNavigationPoints = 1;
const int SiteEngagementScore::kDecayPeriodInDays = 7;
const double SiteEngagementScore::kDecayPoints = 5;

SiteEngagementScore::SiteEngagementScore(base::Clock* clock,
                                         const base::DictionaryValue& settings)
    : SiteEngagementScore(clock) {
  settings.GetDouble(kRawScoreKey, &raw_score_);
  settings.GetDouble(kPointsAddedTodayKey, &points_added_today_);
  double internal_time;
  if (settings.GetDouble(kLastEngagementTimeKey, &internal_time))
    last_engagement_time_ = base::Time::FromInternalValue(internal_time);
}

SiteEngagementScore::~SiteEngagementScore() {
}

double SiteEngagementScore::Score() const {
  return DecayedScore();
}

void SiteEngagementScore::AddPoints(double points) {
  // As the score is about to be updated, commit any decay that has happened
  // since the last update.
  raw_score_ = DecayedScore();

  base::Time now = clock_->Now();
  if (!last_engagement_time_.is_null() &&
      now.LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    points_added_today_ = 0;
  }

  double to_add =
      std::min(kMaxPoints - raw_score_, kMaxPointsPerDay - points_added_today_);
  to_add = std::min(to_add, points);

  points_added_today_ += to_add;
  raw_score_ += to_add;

  last_engagement_time_ = now;
}

bool SiteEngagementScore::UpdateSettings(base::DictionaryValue* settings) {
  double raw_score_orig = 0;
  double points_added_today_orig = 0;
  double last_engagement_time_internal_orig = 0;

  settings->GetDouble(kRawScoreKey, &raw_score_orig);
  settings->GetDouble(kPointsAddedTodayKey, &points_added_today_orig);
  settings->GetDouble(kLastEngagementTimeKey,
                      &last_engagement_time_internal_orig);
  bool changed =
      DoublesConsideredDifferent(raw_score_orig, raw_score_, kScoreDelta) ||
      DoublesConsideredDifferent(points_added_today_orig, points_added_today_,
                                 kScoreDelta) ||
      DoublesConsideredDifferent(last_engagement_time_internal_orig,
                                 last_engagement_time_.ToInternalValue(),
                                 kTimeDelta);

  if (!changed)
    return false;

  settings->SetDouble(kRawScoreKey, raw_score_);
  settings->SetDouble(kPointsAddedTodayKey, points_added_today_);
  settings->SetDouble(kLastEngagementTimeKey,
                      last_engagement_time_.ToInternalValue());

  return true;
}

SiteEngagementScore::SiteEngagementScore(base::Clock* clock)
    : clock_(clock),
      raw_score_(0),
      points_added_today_(0),
      last_engagement_time_() {}

double SiteEngagementScore::DecayedScore() const {
  // Note that users can change their clock, so from this system's perspective
  // time can go backwards. If that does happen and the system detects that the
  // current day is earlier than the last engagement, no decay (or growth) is
  // applied.
  int days_since_engagement = (clock_->Now() - last_engagement_time_).InDays();
  if (days_since_engagement < 0)
    return raw_score_;

  int periods = days_since_engagement / kDecayPeriodInDays;
  double decayed_score = raw_score_ - periods * kDecayPoints;
  return std::max(0.0, decayed_score);
}

// static
SiteEngagementService* SiteEngagementService::Get(Profile* profile) {
  return SiteEngagementServiceFactory::GetForProfile(profile);
}

// static
bool SiteEngagementService::IsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSiteEngagementService);
}

SiteEngagementService::SiteEngagementService(Profile* profile)
    : profile_(profile) {
}

SiteEngagementService::~SiteEngagementService() {
}

void SiteEngagementService::HandleNavigation(const GURL& url) {
  HostContentSettingsMap* settings_map = profile_->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> settings = GetOriginDict(settings_map, url);
  SiteEngagementScore score(&clock_, *settings);

  score.AddPoints(SiteEngagementScore::kNavigationPoints);
  if (score.UpdateSettings(settings.get())) {
    ContentSettingsPattern pattern(
        ContentSettingsPattern::FromURLNoWildcard(url));
    if (!pattern.IsValid())
      return;

    settings_map->SetWebsiteSetting(pattern, ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                                    std::string(), settings.release());
  }
}

int SiteEngagementService::GetScore(const GURL& url) {
  HostContentSettingsMap* settings_map = profile_->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> settings = GetOriginDict(settings_map, url);
  SiteEngagementScore score(&clock_, *settings);

  return score.Score();
}

