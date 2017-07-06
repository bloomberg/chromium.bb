// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/media_engagement_contents_observer.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
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

}  // namespace

// static
bool MediaEngagementService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kMediaEngagement);
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

MediaEngagementService::MediaEngagementService(Profile* profile)
    : MediaEngagementService(profile, base::MakeUnique<base::DefaultClock>()) {}

MediaEngagementService::MediaEngagementService(
    Profile* profile,
    std::unique_ptr<base::Clock> clock)
    : profile_(profile), clock_(std::move(clock)) {
  DCHECK(IsEnabled());
}

MediaEngagementService::~MediaEngagementService() = default;

double MediaEngagementService::GetEngagementScore(const GURL& url) const {
  MediaEngagementScore* score = CreateEngagementScore(url);
  double total_score = score->GetTotalScore();
  delete score;
  return total_score;
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

  MediaEngagementScore* score = CreateEngagementScore(url);
  score->IncrementVisits();
  score->Commit();
  delete score;
}

void MediaEngagementService::RecordPlayback(const GURL& url) {
  if (!ShouldRecordEngagement(url))
    return;

  MediaEngagementScore* score = CreateEngagementScore(url);
  score->IncrementMediaPlaybacks();
  score->Commit();
  delete score;
}

MediaEngagementScore* MediaEngagementService::CreateEngagementScore(
    const GURL& url) const {
  // If we are in incognito, |settings| will automatically have the data from
  // the original profile migrated in, so all engagement scores in incognito
  // will be initialised to the values from the original profile.
  return new MediaEngagementScore(
      clock_.get(), url,
      HostContentSettingsMapFactory::GetForProfile(profile_));
}

bool MediaEngagementService::ShouldRecordEngagement(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS();
}
