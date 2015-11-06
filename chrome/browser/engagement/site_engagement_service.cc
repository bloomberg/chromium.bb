// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_eviction_policy.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

// Global bool to ensure we only update the parameters from variations once.
bool g_updated_from_variations = false;

// Keys used in the variations params.
const char kMaxPointsPerDayParam[] = "max_points_per_day";
const char kNavigationPointsParam[] = "navigation_points";
const char kUserInputPointsParam[] = "user_input_points";
const char kVisibleMediaPlayingPointsParam[] = "visible_media_playing_points";
const char kHiddenMediaPlayingPointsParam[] = "hidden_media_playing_points";
const char kDecayPeriodInDaysParam[] = "decay_period_in_days";
const char kDecayPointsParam[] = "decay_points";

// Length of time between metrics logging.
const base::TimeDelta metrics_interval = base::TimeDelta::FromMinutes(60);

// Delta within which to consider scores equal.
const double kScoreDelta = 0.001;

// Delta within which to consider internal time values equal. Internal time
// values are in microseconds, so this delta comes out at one second.
const double kTimeDelta = 1000000;

scoped_ptr<ContentSettingsForOneType> GetEngagementContentSettings(
    HostContentSettingsMap* settings_map) {
  scoped_ptr<ContentSettingsForOneType> engagement_settings(
      new ContentSettingsForOneType);
  settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                                      std::string(), engagement_settings.get());
  return engagement_settings.Pass();
}

bool DoublesConsideredDifferent(double value1, double value2, double delta) {
  double abs_difference = fabs(value1 - value2);
  return abs_difference > delta;
}

// Only accept a navigation event for engagement if it is one of:
//  a. direct typed navigation
//  b. clicking on an omnibox suggestion brought up by typing a keyword
//  c. clicking on a bookmark or opening a bookmark app
//  d. a custom search engine keyword search (e.g. Wikipedia search box added as
//  search engine).
bool IsEngagementNavigation(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD_GENERATED);
}

scoped_ptr<base::DictionaryValue> GetScoreDictForOrigin(
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

const double SiteEngagementScore::kMaxPoints = 100;
double SiteEngagementScore::g_max_points_per_day = 5;
double SiteEngagementScore::g_navigation_points = 0.5;
double SiteEngagementScore::g_user_input_points = 0.05;
double SiteEngagementScore::g_visible_media_playing_points = 0.02;
double SiteEngagementScore::g_hidden_media_playing_points = 0.01;
int SiteEngagementScore::g_decay_period_in_days = 7;
double SiteEngagementScore::g_decay_points = 5;

const char* SiteEngagementScore::kRawScoreKey = "rawScore";
const char* SiteEngagementScore::kPointsAddedTodayKey = "pointsAddedToday";
const char* SiteEngagementScore::kLastEngagementTimeKey = "lastEngagementTime";

void SiteEngagementScore::UpdateFromVariations() {
  std::string max_points_per_day_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kMaxPointsPerDayParam);
  std::string navigation_points_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kNavigationPointsParam);
  std::string user_input_points_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kUserInputPointsParam);
  std::string visible_media_playing_points_param =
      variations::GetVariationParamValue(
          SiteEngagementService::kEngagementParams,
          kVisibleMediaPlayingPointsParam);
  std::string hidden_media_playing_points_param =
      variations::GetVariationParamValue(
          SiteEngagementService::kEngagementParams,
          kHiddenMediaPlayingPointsParam);
  std::string decay_period_in_days_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kDecayPeriodInDaysParam);
  std::string decay_points_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kDecayPointsParam);

  if (!max_points_per_day_param.empty() && !navigation_points_param.empty() &&
      !user_input_points_param.empty() &&
      !visible_media_playing_points_param.empty() &&
      !hidden_media_playing_points_param.empty() &&
      !decay_period_in_days_param.empty() && !decay_points_param.empty()) {
    double max_points_per_day = 0;
    double navigation_points = 0;
    double user_input_points = 0;
    double visible_media_playing_points = 0;
    double hidden_media_playing_points = 0;
    int decay_period_in_days = 0;
    double decay_points = 0;

    if (base::StringToDouble(max_points_per_day_param, &max_points_per_day) &&
        base::StringToDouble(navigation_points_param, &navigation_points) &&
        base::StringToDouble(user_input_points_param, &user_input_points) &&
        base::StringToDouble(visible_media_playing_points_param,
                             &visible_media_playing_points) &&
        base::StringToDouble(hidden_media_playing_points_param,
                             &hidden_media_playing_points) &&
        base::StringToInt(decay_period_in_days_param, &decay_period_in_days) &&
        base::StringToDouble(decay_points_param, &decay_points) &&
        max_points_per_day >= navigation_points &&
        max_points_per_day >= user_input_points && navigation_points >= 0 &&
        user_input_points >= 0 && decay_period_in_days > 0 &&
        decay_points >= 0) {
      g_max_points_per_day = max_points_per_day;
      g_navigation_points = navigation_points;
      g_user_input_points = user_input_points;
      g_visible_media_playing_points = visible_media_playing_points;
      g_hidden_media_playing_points = hidden_media_playing_points;
      g_decay_period_in_days = decay_period_in_days;
      g_decay_points = decay_points;
    }
  }
}

SiteEngagementScore::SiteEngagementScore(
    base::Clock* clock,
    const base::DictionaryValue& score_dict)
    : SiteEngagementScore(clock) {
  score_dict.GetDouble(kRawScoreKey, &raw_score_);
  score_dict.GetDouble(kPointsAddedTodayKey, &points_added_today_);
  double internal_time;
  if (score_dict.GetDouble(kLastEngagementTimeKey, &internal_time))
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

  double to_add = std::min(kMaxPoints - raw_score_,
                           g_max_points_per_day - points_added_today_);
  to_add = std::min(to_add, points);

  points_added_today_ += to_add;
  raw_score_ += to_add;

  last_engagement_time_ = now;
}

bool SiteEngagementScore::MaxPointsPerDayAdded() {
  if (!last_engagement_time_.is_null() &&
      clock_->Now().LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    return false;
  }

  return points_added_today_ == g_max_points_per_day;
}

bool SiteEngagementScore::UpdateScoreDict(base::DictionaryValue* score_dict) {
  double raw_score_orig = 0;
  double points_added_today_orig = 0;
  double last_engagement_time_internal_orig = 0;

  score_dict->GetDouble(kRawScoreKey, &raw_score_orig);
  score_dict->GetDouble(kPointsAddedTodayKey, &points_added_today_orig);
  score_dict->GetDouble(kLastEngagementTimeKey,
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

  score_dict->SetDouble(kRawScoreKey, raw_score_);
  score_dict->SetDouble(kPointsAddedTodayKey, points_added_today_);
  score_dict->SetDouble(kLastEngagementTimeKey,
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

  int periods = days_since_engagement / g_decay_period_in_days;
  double decayed_score = raw_score_ - periods * g_decay_points;
  return std::max(0.0, decayed_score);
}

const char SiteEngagementService::kEngagementParams[] = "SiteEngagement";

// static
SiteEngagementService* SiteEngagementService::Get(Profile* profile) {
  return SiteEngagementServiceFactory::GetForProfile(profile);
}

// static
bool SiteEngagementService::IsEnabled() {
  // If the engagement service or any of its dependencies are force-enabled,
  // return true immediately.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSiteEngagementService) ||
      SiteEngagementEvictionPolicy::IsEnabled()) {
    return true;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteEngagementService)) {
    return false;
  }
  const std::string group_name =
      base::FieldTrialList::FindFullName(kEngagementParams);
  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

// static
void SiteEngagementService::ClearHistoryForURLs(Profile* profile,
                                                const std::set<GURL>& origins) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  for (const GURL& origin_url : origins) {
    ContentSettingsPattern pattern(
        ContentSettingsPattern::FromURLNoWildcard(origin_url));
    if (!pattern.IsValid())
      continue;

    settings_map->SetWebsiteSetting(pattern, ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                                    std::string(), nullptr);
  }
  settings_map->FlushLossyWebsiteSettings();
}

SiteEngagementService::SiteEngagementService(Profile* profile)
    : SiteEngagementService(profile, make_scoped_ptr(new base::DefaultClock)) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetMessageLoopProxyForThread(
                     content::BrowserThread::UI),
      base::Bind(&SiteEngagementService::AfterStartupTask,
                 weak_factory_.GetWeakPtr()));

  if (!g_updated_from_variations) {
    SiteEngagementScore::UpdateFromVariations();
    g_updated_from_variations = true;
  }
}

SiteEngagementService::~SiteEngagementService() {
}

void SiteEngagementService::HandleNavigation(const GURL& url,
                                             ui::PageTransition transition) {
  if (IsEngagementNavigation(transition)) {
    SiteEngagementMetrics::RecordEngagement(
        SiteEngagementMetrics::ENGAGEMENT_NAVIGATION);
    AddPoints(url, SiteEngagementScore::g_navigation_points);
    RecordMetrics();
  }
}

void SiteEngagementService::HandleUserInput(
    const GURL& url,
    SiteEngagementMetrics::EngagementType type) {
  SiteEngagementMetrics::RecordEngagement(type);
  AddPoints(url, SiteEngagementScore::g_user_input_points);
  RecordMetrics();
}

void SiteEngagementService::HandleMediaPlaying(const GURL& url,
                                               bool is_hidden) {
  SiteEngagementMetrics::RecordEngagement(
      is_hidden ? SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN
                : SiteEngagementMetrics::ENGAGEMENT_MEDIA_VISIBLE);
  AddPoints(url, is_hidden
                     ? SiteEngagementScore::g_hidden_media_playing_points
                     : SiteEngagementScore::g_visible_media_playing_points);
  RecordMetrics();
}

double SiteEngagementService::GetScore(const GURL& url) {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  scoped_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore score(clock_.get(), *score_dict);

  return score.Score();
}

double SiteEngagementService::GetTotalEngagementPoints() {
  std::map<GURL, double> score_map = GetScoreMap();

  double total_score = 0;
  for (const auto& value : score_map)
    total_score += value.second;

  return total_score;
}

std::map<GURL, double> SiteEngagementService::GetScoreMap() {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  scoped_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  std::map<GURL, double> score_map;
  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;

    scoped_ptr<base::DictionaryValue> score_dict =
        GetScoreDictForOrigin(settings_map, origin);
    SiteEngagementScore score(clock_.get(), *score_dict);
    score_map[origin] = score.Score();
  }

  return score_map;
}

SiteEngagementService::SiteEngagementService(Profile* profile,
                                             scoped_ptr<base::Clock> clock)
    : profile_(profile), clock_(clock.Pass()), weak_factory_(this) {}

void SiteEngagementService::AddPoints(const GURL& url, double points) {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  scoped_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore score(clock_.get(), *score_dict);

  score.AddPoints(points);
  if (score.UpdateScoreDict(score_dict.get())) {
    ContentSettingsPattern pattern(
        ContentSettingsPattern::FromURLNoWildcard(url));
    if (!pattern.IsValid())
      return;

    settings_map->SetWebsiteSetting(pattern, ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                                    std::string(), score_dict.release());
  }
}

void SiteEngagementService::AfterStartupTask() {
  CleanupEngagementScores();
  RecordMetrics();
}

void SiteEngagementService::CleanupEngagementScores() {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  scoped_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (origin.is_valid()) {
      scoped_ptr<base::DictionaryValue> score_dict =
          GetScoreDictForOrigin(settings_map, origin);
      SiteEngagementScore score(clock_.get(), *score_dict);
      if (score.Score() != 0)
        continue;
    }

    settings_map->SetWebsiteSetting(
        site.primary_pattern, ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), nullptr);
  }
}

void SiteEngagementService::RecordMetrics() {
  base::Time now = clock_->Now();
  if (last_metrics_time_.is_null() ||
      now - last_metrics_time_ >= metrics_interval) {
    last_metrics_time_ = now;
    std::map<GURL, double> score_map = GetScoreMap();

    int origins_with_max_engagement = OriginsWithMaxEngagement(score_map);
    int total_origins = score_map.size();
    int percent_origins_with_max_engagement =
        (total_origins == 0 ? 0 : (origins_with_max_engagement * 100) /
                                      total_origins);

    double total_engagement = GetTotalEngagementPoints();
    double mean_engagement =
        (total_origins == 0 ? 0 : total_engagement / total_origins);

    SiteEngagementMetrics::RecordTotalOriginsEngaged(total_origins);
    SiteEngagementMetrics::RecordTotalSiteEngagement(total_engagement);
    SiteEngagementMetrics::RecordMeanEngagement(mean_engagement);
    SiteEngagementMetrics::RecordMedianEngagement(
        GetMedianEngagement(score_map));
    SiteEngagementMetrics::RecordEngagementScores(score_map);

    SiteEngagementMetrics::RecordOriginsWithMaxDailyEngagement(
        OriginsWithMaxDailyEngagement());
    SiteEngagementMetrics::RecordOriginsWithMaxEngagement(
        origins_with_max_engagement);
    SiteEngagementMetrics::RecordPercentOriginsWithMaxEngagement(
        percent_origins_with_max_engagement);
  }
}

double SiteEngagementService::GetMedianEngagement(
    std::map<GURL, double>& score_map) {
  if (score_map.size() == 0)
    return 0;

  std::vector<double> scores;
  scores.reserve(score_map.size());
  for (const auto& value : score_map)
    scores.push_back(value.second);

  // Calculate the median as the middle value of the sorted engagement scores
  // if there are an odd number of scores, or the average of the two middle
  // scores otherwise.
  std::sort(scores.begin(), scores.end());
  size_t mid = scores.size() / 2;
  if (scores.size() % 2 == 1)
    return scores[mid];
  else
    return (scores[mid - 1] + scores[mid]) / 2;
}

int SiteEngagementService::OriginsWithMaxDailyEngagement() {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  scoped_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  int total_origins = 0;

  // We cannot call GetScoreMap as we need the score objects, not raw scores.
  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;

    scoped_ptr<base::DictionaryValue> score_dict =
        GetScoreDictForOrigin(settings_map, origin);
    SiteEngagementScore score(clock_.get(), *score_dict);
    if (score.MaxPointsPerDayAdded())
      ++total_origins;
  }

  return total_origins;
}

int SiteEngagementService::OriginsWithMaxEngagement(
    std::map<GURL, double>& score_map) {
  int total_origins = 0;

  for (const auto& value : score_map)
    if (value.second == SiteEngagementScore::kMaxPoints)
      ++total_origins;

  return total_origins;
}
