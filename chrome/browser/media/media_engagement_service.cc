// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/media_engagement_contents_observer.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace {

// Returns the combined list of origins which have media engagement data.
std::set<GURL> GetEngagementOriginsFromContentSettings(Profile* profile) {
  ContentSettingsForOneType content_settings;
  std::set<GURL> urls;

  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT,
      content_settings::ResourceIdentifier(), &content_settings);

  for (const auto& site : content_settings)
    urls.insert(GURL(site.primary_pattern.ToString()));

  return urls;
}

bool MediaEngagementFilterAdapter(
    const GURL& predicate,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  GURL url(primary_pattern.ToString());
  DCHECK(url.is_valid());
  return predicate == url;
}

bool MediaEngagementTimeFilterAdapter(
    MediaEngagementService* service,
    base::Time delete_begin,
    base::Time delete_end,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  GURL url(primary_pattern.ToString());
  DCHECK(url.is_valid());
  MediaEngagementScore score = service->CreateEngagementScore(url);
  base::Time playback_time = score.last_media_playback_time();
  return playback_time >= delete_begin && playback_time <= delete_end;
}

// The current schema version of the MEI data. If this value is higher
// than the stored value, all MEI data will be wiped.
static const int kSchemaVersion = 3;

}  // namespace

const char MediaEngagementService::kHistogramScoreAtStartupName[] =
    "Media.Engagement.ScoreAtStartup";

// static
bool MediaEngagementService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kRecordMediaEngagementScores);
}

// static
MediaEngagementService* MediaEngagementService::Get(Profile* profile) {
  DCHECK(IsEnabled());
  return MediaEngagementServiceFactory::GetForProfile(profile);
}

// static
void MediaEngagementService::CreateWebContentsObserver(
    content::WebContents* web_contents) {
  DCHECK(IsEnabled());
  MediaEngagementService* service =
      Get(Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service)
    return;
  service->contents_observers_.insert(
      new MediaEngagementContentsObserver(web_contents, service));
}

// static
void MediaEngagementService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kMediaEngagementSchemaVersion, 0, 0);
}

MediaEngagementService::MediaEngagementService(Profile* profile)
    : MediaEngagementService(profile, base::MakeUnique<base::DefaultClock>()) {}

MediaEngagementService::MediaEngagementService(
    Profile* profile,
    std::unique_ptr<base::Clock> clock)
    : profile_(profile), clock_(std::move(clock)) {
  DCHECK(IsEnabled());

  // May be null in tests.
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->AddObserver(this);

  // If kSchemaVersion is higher than what we have stored we should wipe
  // all Media Engagement data.
  if (GetSchemaVersion() < kSchemaVersion) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT);
    SetSchemaVersion(kSchemaVersion);
  }

  // Record the stored scores to a histogram.
  RecordStoredScoresToHistogram();
}

MediaEngagementService::~MediaEngagementService() = default;

int MediaEngagementService::GetSchemaVersion() const {
  return profile_->GetPrefs()->GetInteger(prefs::kMediaEngagementSchemaVersion);
}

void MediaEngagementService::SetSchemaVersion(int version) {
  return profile_->GetPrefs()->SetInteger(prefs::kMediaEngagementSchemaVersion,
                                          version);
}

void MediaEngagementService::ClearDataBetweenTime(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->ClearSettingsForOneTypeWithPredicate(
          CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, base::Time(),
          base::Bind(&MediaEngagementTimeFilterAdapter, this, delete_begin,
                     delete_end));
}

void MediaEngagementService::Shutdown() {
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  if (history)
    history->RemoveObserver(this);
}

void MediaEngagementService::RecordStoredScoresToHistogram() {
  for (const GURL& url : GetEngagementOriginsFromContentSettings(profile_)) {
    if (!url.is_valid())
      continue;

    int percentage = round(GetEngagementScore(url) * 100);
    UMA_HISTOGRAM_PERCENTAGE(
        MediaEngagementService::kHistogramScoreAtStartupName, percentage);
  }
}

void MediaEngagementService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  std::map<GURL, int> origins;
  for (const history::URLRow& row : deleted_rows) {
    GURL origin = row.url().GetOrigin();
    if (origins.find(origin) == origins.end()) {
      origins[origin] = 0;
    }
    origins[origin]++;
  }

  for (auto const& kv : origins) {
    // Remove the number of visits consistent with the number
    // of URLs from the same origin we are removing.
    MediaEngagementScore score = CreateEngagementScore(kv.first);
    double original_score = score.actual_score();
    score.SetVisits(score.visits() - kv.second);

    // If this results in zero visits then clear the score.
    if (score.visits() <= 0) {
      Clear(kv.first);
      continue;
    }

    // Otherwise, recalculate the playbacks to keep the
    // MEI score consistent.
    score.SetMediaPlaybacks(original_score * score.visits());
    score.Commit();
  }
}

void MediaEngagementService::Clear(const GURL& url) {
  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->ClearSettingsForOneTypeWithPredicate(
          CONTENT_SETTINGS_TYPE_MEDIA_ENGAGEMENT, base::Time(),
          base::Bind(&MediaEngagementFilterAdapter, base::ConstRef(url)));
}

double MediaEngagementService::GetEngagementScore(const GURL& url) const {
  return CreateEngagementScore(url).actual_score();
}

bool MediaEngagementService::HasHighEngagement(const GURL& url) const {
  return CreateEngagementScore(url).high_score();
}

std::map<GURL, double> MediaEngagementService::GetScoreMapForTesting() const {
  std::map<GURL, double> score_map;
  for (const GURL& url : GetEngagementOriginsFromContentSettings(profile_)) {
    if (!url.is_valid())
      continue;
    score_map[url] = GetEngagementScore(url);
  }
  return score_map;
}

void MediaEngagementService::RecordVisit(const GURL& url) {
  if (!ShouldRecordEngagement(url))
    return;

  MediaEngagementScore score = CreateEngagementScore(url);
  score.IncrementVisits();
  score.Commit();
}

std::vector<media::mojom::MediaEngagementScoreDetailsPtr>
MediaEngagementService::GetAllScoreDetails() const {
  std::set<GURL> origins = GetEngagementOriginsFromContentSettings(profile_);

  std::vector<media::mojom::MediaEngagementScoreDetailsPtr> details;
  details.reserve(origins.size());
  for (const GURL& origin : origins) {
    // TODO(beccahughes): Why would an origin not be valid here?
    if (!origin.is_valid())
      continue;
    MediaEngagementScore score = CreateEngagementScore(origin);
    details.push_back(score.GetScoreDetails());
  }

  return details;
}

void MediaEngagementService::RecordPlayback(const GURL& url) {
  if (!ShouldRecordEngagement(url))
    return;

  MediaEngagementScore score = CreateEngagementScore(url);
  score.IncrementMediaPlaybacks();
  score.Commit();
}

MediaEngagementScore MediaEngagementService::CreateEngagementScore(
    const GURL& url) const {
  // If we are in incognito, |settings| will automatically have the data from
  // the original profile migrated in, so all engagement scores in incognito
  // will be initialised to the values from the original profile.
  return MediaEngagementScore(
      clock_.get(), url,
      HostContentSettingsMapFactory::GetForProfile(profile_));
}

bool MediaEngagementService::ShouldRecordEngagement(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS();
}
