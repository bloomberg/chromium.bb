// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_eviction_policy.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

const int FOUR_WEEKS_IN_DAYS = 28;

// Global bool to ensure we only update the parameters from variations once.
bool g_updated_from_variations = false;

// Length of time between metrics logging.
const int kMetricsIntervalInMinutes = 60;

std::unique_ptr<ContentSettingsForOneType> GetEngagementContentSettings(
    HostContentSettingsMap* settings_map) {
  std::unique_ptr<ContentSettingsForOneType> engagement_settings(
      new ContentSettingsForOneType);
  settings_map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                                      std::string(), engagement_settings.get());
  return engagement_settings;
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

}  // namespace

const char SiteEngagementService::kEngagementParams[] = "SiteEngagement";

// static
SiteEngagementService* SiteEngagementService::Get(Profile* profile) {
  return SiteEngagementServiceFactory::GetForProfile(profile);
}

// static
double SiteEngagementService::GetMaxPoints() {
  return SiteEngagementScore::kMaxPoints;
}

// static
bool SiteEngagementService::IsEnabled() {
  // If the engagement service or any of its dependencies are force-enabled,
  // return true immediately.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSiteEngagementService) ||
      SiteEngagementEvictionPolicy::IsEnabled() ||
      AppBannerSettingsHelper::ShouldUseSiteEngagementScore()) {
    return true;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSiteEngagementService)) {
    return false;
  }
  const std::string group_name =
      base::FieldTrialList::FindFullName(kEngagementParams);
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::SENSITIVE);
}

SiteEngagementService::SiteEngagementService(Profile* profile)
    : SiteEngagementService(profile, base::WrapUnique(new base::DefaultClock)) {
  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetMessageLoopProxyForThread(
                     content::BrowserThread::UI),
      base::Bind(&SiteEngagementService::AfterStartupTask,
                 weak_factory_.GetWeakPtr()));

  if (!g_updated_from_variations) {
    SiteEngagementScore::UpdateFromVariations(kEngagementParams);
    g_updated_from_variations = true;
  }
}

SiteEngagementService::~SiteEngagementService() {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->RemoveObserver(this);
}

SiteEngagementService::EngagementLevel
SiteEngagementService::GetEngagementLevel(const GURL& url) const {
  DCHECK_LT(SiteEngagementScore::GetMediumEngagementBoundary(),
            SiteEngagementScore::GetHighEngagementBoundary());
  double score = GetScore(url);
  if (score == 0)
    return ENGAGEMENT_LEVEL_NONE;

  if (score < SiteEngagementScore::GetMediumEngagementBoundary())
    return ENGAGEMENT_LEVEL_LOW;

  if (score < SiteEngagementScore::GetHighEngagementBoundary())
    return ENGAGEMENT_LEVEL_MEDIUM;

  if (score < SiteEngagementScore::kMaxPoints)
    return ENGAGEMENT_LEVEL_HIGH;

  return ENGAGEMENT_LEVEL_MAX;
}

std::map<GURL, double> SiteEngagementService::GetScoreMap() const {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  std::map<GURL, double> score_map;
  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;

    std::unique_ptr<base::DictionaryValue> score_dict =
        GetScoreDictForOrigin(settings_map, origin);
    SiteEngagementScore score(clock_.get(), *score_dict);
    score_map[origin] = score.GetScore();
  }

  return score_map;
}

bool SiteEngagementService::IsBootstrapped() {
  return GetTotalEngagementPoints() >=
         SiteEngagementScore::GetBootstrapPoints();
}

bool SiteEngagementService::IsEngagementAtLeast(
    const GURL& url,
    EngagementLevel level) const {
  DCHECK_LT(SiteEngagementScore::GetMediumEngagementBoundary(),
            SiteEngagementScore::GetHighEngagementBoundary());
  double score = GetScore(url);
  switch (level) {
    case ENGAGEMENT_LEVEL_NONE:
      return true;
    case ENGAGEMENT_LEVEL_LOW:
      return score > 0;
    case ENGAGEMENT_LEVEL_MEDIUM:
      return score >= SiteEngagementScore::GetMediumEngagementBoundary();
    case ENGAGEMENT_LEVEL_HIGH:
      return score >= SiteEngagementScore::GetHighEngagementBoundary();
    case ENGAGEMENT_LEVEL_MAX:
      return score == SiteEngagementScore::kMaxPoints;
  }
  NOTREACHED();
  return false;
}

void SiteEngagementService::ResetScoreForURL(const GURL& url, double score) {
  ResetScoreAndAccessTimesForURL(url, score, nullptr);
}

void SiteEngagementService::SetLastShortcutLaunchTime(const GURL& url) {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore score(clock_.get(), *score_dict);

  // Record the number of days since the last launch in UMA. If the user's clock
  // has changed back in time, set this to 0.
  base::Time now = clock_->Now();
  base::Time last_launch = score.last_shortcut_launch_time();
  if (!last_launch.is_null()) {
    SiteEngagementMetrics::RecordDaysSinceLastShortcutLaunch(
        std::max(0, (now - last_launch).InDays()));
  }
  SiteEngagementMetrics::RecordEngagement(
      SiteEngagementMetrics::ENGAGEMENT_WEBAPP_SHORTCUT_LAUNCH);

  score.set_last_shortcut_launch_time(now);
  if (score.UpdateScoreDict(score_dict.get())) {
    settings_map->SetWebsiteSettingDefaultScope(
        url, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
        score_dict.release());
  }
}

double SiteEngagementService::GetScore(const GURL& url) const {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore score(clock_.get(), *score_dict);

  return score.GetScore();
}

double SiteEngagementService::GetTotalEngagementPoints() const {
  std::map<GURL, double> score_map = GetScoreMap();

  double total_score = 0;
  for (const auto& value : score_map)
    total_score += value.second;

  return total_score;
}

SiteEngagementService::SiteEngagementService(Profile* profile,
                                             std::unique_ptr<base::Clock> clock)
    : profile_(profile), clock_(std::move(clock)), weak_factory_(this) {
  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->AddObserver(this);
}

void SiteEngagementService::AddPoints(const GURL& url, double points) {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore score(clock_.get(), *score_dict);

  score.AddPoints(points);
  if (score.UpdateScoreDict(score_dict.get())) {
    settings_map->SetWebsiteSettingDefaultScope(
        url, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
        score_dict.release());
  }
}

void SiteEngagementService::AfterStartupTask() {
  CleanupEngagementScores();
  RecordMetrics();
}

void SiteEngagementService::CleanupEngagementScores() {
  HostContentSettingsMap* settings_map =
    HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (origin.is_valid()) {
      std::unique_ptr<base::DictionaryValue> score_dict =
          GetScoreDictForOrigin(settings_map, origin);
      SiteEngagementScore score(clock_.get(), *score_dict);
      if (score.GetScore() != 0)
        continue;
    }

    settings_map->SetWebsiteSettingDefaultScope(
        origin, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
        nullptr);
  }
}

void SiteEngagementService::RecordMetrics() {
  base::Time now = clock_->Now();
  if (last_metrics_time_.is_null() ||
      (now - last_metrics_time_).InMinutes() >= kMetricsIntervalInMinutes) {
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
    const std::map<GURL, double>& score_map) const {
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

void SiteEngagementService::HandleMediaPlaying(const GURL& url,
                                               bool is_hidden) {
  SiteEngagementMetrics::RecordEngagement(
      is_hidden ? SiteEngagementMetrics::ENGAGEMENT_MEDIA_HIDDEN
                : SiteEngagementMetrics::ENGAGEMENT_MEDIA_VISIBLE);
  AddPoints(url, is_hidden ? SiteEngagementScore::GetHiddenMediaPoints()
                           : SiteEngagementScore::GetVisibleMediaPoints());
  RecordMetrics();
}

void SiteEngagementService::HandleNavigation(const GURL& url,
                                             ui::PageTransition transition) {
  if (IsEngagementNavigation(transition)) {
    SiteEngagementMetrics::RecordEngagement(
        SiteEngagementMetrics::ENGAGEMENT_NAVIGATION);
    AddPoints(url, SiteEngagementScore::GetNavigationPoints());
    RecordMetrics();
  }
}

void SiteEngagementService::HandleUserInput(
    const GURL& url,
    SiteEngagementMetrics::EngagementType type) {
  SiteEngagementMetrics::RecordEngagement(type);
  AddPoints(url, SiteEngagementScore::GetUserInputPoints());
  RecordMetrics();
}

void SiteEngagementService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  std::multiset<GURL> origins;
  for (const history::URLRow& row : deleted_rows)
    origins.insert(row.url().GetOrigin());

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  hs->GetCountsAndLastVisitForOrigins(
      std::set<GURL>(origins.begin(), origins.end()),
      base::Bind(
          &SiteEngagementService::GetCountsAndLastVisitForOriginsComplete,
          weak_factory_.GetWeakPtr(), hs, origins, expired));
}

int SiteEngagementService::OriginsWithMaxDailyEngagement() const {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<ContentSettingsForOneType> engagement_settings =
      GetEngagementContentSettings(settings_map);

  int total_origins = 0;

  // We cannot call GetScoreMap as we need the score objects, not raw scores.
  for (const auto& site : *engagement_settings) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;

    std::unique_ptr<base::DictionaryValue> score_dict =
        GetScoreDictForOrigin(settings_map, origin);
    SiteEngagementScore score(clock_.get(), *score_dict);
    if (score.MaxPointsPerDayAdded())
      ++total_origins;
  }

  return total_origins;
}

int SiteEngagementService::OriginsWithMaxEngagement(
    const std::map<GURL, double>& score_map) const {
  int total_origins = 0;

  for (const auto& value : score_map)
    if (value.second == SiteEngagementScore::kMaxPoints)
      ++total_origins;

  return total_origins;
}

void SiteEngagementService::GetCountsAndLastVisitForOriginsComplete(
    history::HistoryService* history_service,
    const std::multiset<GURL>& deleted_origins,
    bool expired,
    const history::OriginCountAndLastVisitMap& remaining_origins) {

  // The most in-the-past option in the Clear Browsing Dialog aside from "all
  // time" is 4 weeks ago. Set the last updated date to 4 weeks ago for origins
  // where we can't find a valid last visit date.
  base::Time now = clock_->Now();
  base::Time four_weeks_ago =
      now - base::TimeDelta::FromDays(FOUR_WEEKS_IN_DAYS);

  for (const auto& origin_to_count : remaining_origins) {
    GURL origin = origin_to_count.first;
    int remaining = origin_to_count.second.first;
    base::Time last_visit = origin_to_count.second.second;
    int deleted = deleted_origins.count(origin);

    // Do not update engagement scores if the deletion was an expiry, but the
    // URL still has entries in history.
    if ((expired && remaining != 0) || deleted == 0)
      continue;

    // Remove engagement proportional to the urls expired from the origin's
    // entire history.
    double proportion_remaining =
        static_cast<double>(remaining) / (remaining + deleted);
    if (last_visit.is_null() || last_visit > now)
      last_visit = four_weeks_ago;

    // At this point, we are going to proportionally decay the origin's
    // engagement, and reset its last visit date to the last visit to a URL
    // under the origin in history. If this new last visit date is long enough
    // in the past, the next time the origin's engagement is accessed the
    // automatic decay will kick in - i.e. a double decay will have occurred.
    // To prevent this, compute the decay that would have taken place since the
    // new last visit and add it to the engagement at this point. When the
    // engagement is next accessed, it will decay back to the proportionally
    // reduced value rather than being decayed once here, and then once again
    // when it is next accessed.
    int undecay = 0;
    int days_since_engagement = (now - last_visit).InDays();
    if (days_since_engagement > 0) {
      int periods = days_since_engagement /
                    SiteEngagementScore::GetDecayPeriodInDays();
      undecay = periods * SiteEngagementScore::GetDecayPoints();
    }

    double score =
        std::min(SiteEngagementScore::kMaxPoints,
                 (proportion_remaining * GetScore(origin)) + undecay);
    ResetScoreAndAccessTimesForURL(origin, score, &last_visit);
  }
}

void SiteEngagementService::ResetScoreAndAccessTimesForURL(
    const GURL& url, double score, const base::Time* updated_time) {
  DCHECK(url.is_valid());
  DCHECK_GE(score, 0);
  DCHECK_LE(score, SiteEngagementScore::kMaxPoints);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> score_dict =
      GetScoreDictForOrigin(settings_map, url);
  SiteEngagementScore engagement_score(clock_.get(), *score_dict);

  engagement_score.Reset(score, updated_time);
  if (score == 0) {
    settings_map->SetWebsiteSettingDefaultScope(
        url, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
        nullptr);
    return;
  }

  if (engagement_score.UpdateScoreDict(score_dict.get())) {
    settings_map->SetWebsiteSettingDefaultScope(
        url, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
        score_dict.release());
  }
}
