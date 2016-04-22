// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/important_sites_util.h"

#include <algorithm>
#include <map>
#include <set>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

std::vector<std::pair<GURL, double>> GetSortedTopEngagementOrigins(
    const SiteEngagementService* site_engagement_service,
    const std::map<GURL, double>& engagement_map,
    SiteEngagementService::EngagementLevel minimum_engagement) {
  std::vector<std::pair<GURL, double>> top_ranking_origins;
  for (const auto& url_engagement_pair : engagement_map) {
    if (!site_engagement_service->IsEngagementAtLeast(url_engagement_pair.first,
                                                      minimum_engagement)) {
      continue;
    }
    top_ranking_origins.push_back(url_engagement_pair);
  }
  std::sort(
      top_ranking_origins.begin(), top_ranking_origins.end(),
      [](const std::pair<GURL, double>& a, const std::pair<GURL, double>& b) {
        return a.second > b.second;
      });
  return top_ranking_origins;
}

std::vector<std::pair<GURL, double>> GenerateSortedOriginsForContentTypeAllowed(
    Profile* profile,
    ContentSettingsType content_type,
    const std::map<GURL, double>& score_map) {
  // Grab our content settings list.
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      content_type, content_settings::ResourceIdentifier(),
      &content_settings_list);
  // Extract a set of urls, using the primary pattern. We don't handle wildcard
  // patterns.
  std::set<GURL> content_origins;
  for (const ContentSettingPatternSource& site : content_settings_list) {
    if (site.setting != CONTENT_SETTING_ALLOW)
      continue;
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;
    content_origins.insert(origin);
  }
  std::vector<std::pair<GURL, double>> top_ranking_origins;
  for (const GURL& notification_origin : content_origins) {
    double score = 0;
    auto score_it = score_map.find(notification_origin);
    if (score_it != score_map.end())
      score = score_it->second;
    top_ranking_origins.push_back(std::make_pair(notification_origin, score));
  }
  std::sort(
      top_ranking_origins.begin(), top_ranking_origins.end(),
      [](const std::pair<GURL, double>& a, const std::pair<GURL, double>& b) {
        return a.second > b.second;
      });
  return top_ranking_origins;
}

void FillTopRegisterableDomains(
    const std::vector<std::pair<GURL, double>>& sorted_new_origins,
    size_t max_important_domains,
    std::vector<std::string>* final_list) {
  for (const auto& pair : sorted_new_origins) {
    if (final_list->size() >= max_important_domains)
      return;
    std::string registerable_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            pair.first,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    // Just iterate to find, as we assume our size is small.
    if (std::find(final_list->begin(), final_list->end(),
                  registerable_domain) == final_list->end()) {
      final_list->push_back(registerable_domain);
    }
  }
}

}  // namespace

std::vector<std::string> ImportantSitesUtil::GetImportantRegisterableDomains(
    Profile* profile,
    size_t max_results) {
  // First get data from site engagement.
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);
  // Engagement data.
  std::map<GURL, double> engagement_map =
      site_engagement_service->GetScoreMap();
  std::vector<std::pair<GURL, double>> sorted_engagement_origins =
      GetSortedTopEngagementOrigins(
          site_engagement_service, engagement_map,
          SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM);

  // Second we grab origins with push notifications.
  std::vector<std::pair<GURL, double>> sorted_notification_origins =
      GenerateSortedOriginsForContentTypeAllowed(
          profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, engagement_map);

  // Now we transform them into registerable domains, and add them into our
  // final list. Since our # is small, we just iterate our vector to de-dup.
  // Otherwise we can add a set later.
  std::vector<std::string> final_list;
  // We start with notifications.
  FillTopRegisterableDomains(sorted_notification_origins, max_results,
                             &final_list);
  // And now we fill the rest with high engagement sites.
  FillTopRegisterableDomains(sorted_engagement_origins, max_results,
                             &final_list);
  return final_list;
}
