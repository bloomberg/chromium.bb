// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/important_sites_util.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

// Do not change the values here, as they are used for UMA histograms.
enum ReasonStatTypes {
  DURABLE = 0,
  NOTIFICATIONS,
  ENGAGEMENT,
  NOTIFICATIONS_AND_ENGAGEMENT,
  DURABLE_AND_ENGAGEMENT,
  NOTIFICATIONS_AND_DURABLE,
  NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT,
  REASON_UNKNOWN,
  REASON_BOUNDARY
};

struct ImportantReason {
  bool engagement = false;
  bool notifications = false;
  bool durable = false;
};

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
    std::vector<std::string>* final_list,
    std::vector<GURL>* optional_example_origins) {
  for (const auto& pair : sorted_new_origins) {
    if (final_list->size() >= max_important_domains)
      return;
    std::string registerable_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            pair.first,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (registerable_domain.empty() && pair.first.HostIsIPAddress())
      registerable_domain = pair.first.host();
    // Just iterate to find, as we assume our size is small.
    if (std::find(final_list->begin(), final_list->end(),
                  registerable_domain) == final_list->end()) {
      final_list->push_back(registerable_domain);
      if (optional_example_origins)
        optional_example_origins->push_back(pair.first);
    }
  }
}

}  // namespace

std::vector<std::string> ImportantSitesUtil::GetImportantRegisterableDomains(
    Profile* profile,
    size_t max_results,
    std::vector<GURL>* optional_example_origins) {
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

  // Second we grab origins with desired content settings.
  std::vector<std::pair<GURL, double>> sorted_notification_origins =
      GenerateSortedOriginsForContentTypeAllowed(
          profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS, engagement_map);

  std::vector<std::pair<GURL, double>> sorted_durable_origins =
      GenerateSortedOriginsForContentTypeAllowed(
          profile, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, engagement_map);

  // Now we transform them into registerable domains, and add them into our
  // final list. Since our # is small, we just iterate our vector to de-dup.
  // Otherwise we can add a set later.
  std::vector<std::string> final_list;
  // We include sites in the following order:
  // 1. Durable
  // 2. Notifications
  FillTopRegisterableDomains(sorted_durable_origins, max_results,
                             &final_list, optional_example_origins);
  FillTopRegisterableDomains(sorted_notification_origins, max_results,
                             &final_list, optional_example_origins);

  // And now we fill the rest with high engagement sites.
  FillTopRegisterableDomains(sorted_engagement_origins, max_results,
                             &final_list, optional_example_origins);
  return final_list;
}

void ImportantSitesUtil::RecordMetricsForBlacklistedSites(
    Profile* profile,
    std::vector<std::string> blacklisted_sites) {
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);

  std::map<std::string, ImportantReason> reason_map;

  std::map<GURL, double> engagement_map =
      site_engagement_service->GetScoreMap();

  // Site engagement.
  for (const auto& url_score_pair : engagement_map) {
    if (url_score_pair.second <
        SiteEngagementScore::GetMediumEngagementBoundary()) {
      continue;
    }
    const std::string& host = url_score_pair.first.host();
    for (const std::string& blacklisted_site : blacklisted_sites) {
      if (host.find(blacklisted_site) != std::string::npos) {
        reason_map[blacklisted_site].engagement |= true;
        break;
      }
    }
  }

  // Durable.
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
      content_settings::ResourceIdentifier(), &content_settings_list);
  for (const ContentSettingPatternSource& site : content_settings_list) {
    if (site.setting != CONTENT_SETTING_ALLOW)
      continue;
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;
    const std::string& host = origin.host();
    for (const std::string& blacklisted_site : blacklisted_sites) {
      if (host.find(blacklisted_site) != std::string::npos) {
        reason_map[blacklisted_site].durable |= true;
        break;
      }
    }
  }

  // Notifications.
  content_settings_list.clear();
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      content_settings::ResourceIdentifier(), &content_settings_list);
  for (const ContentSettingPatternSource& site : content_settings_list) {
    if (site.setting != CONTENT_SETTING_ALLOW)
      continue;
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid())
      continue;
    const std::string& host = origin.host();
    for (const std::string& blacklisted_site : blacklisted_sites) {
      if (host.find(blacklisted_site) != std::string::npos) {
        reason_map[blacklisted_site].notifications |= true;
        break;
      }
    }
  }

  // Note: we don't plan on adding new metrics here, this is just for the finch
  // experiment to give us initial data on what signals actually mattered.
  for (const auto& reason_pair : reason_map) {
    const ImportantReason& reason = reason_pair.second;
    if (reason.notifications && reason.durable && reason.engagement) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT,
                                REASON_BOUNDARY);
    } else if (reason.notifications && reason.durable) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                NOTIFICATIONS_AND_DURABLE, REASON_BOUNDARY);
    } else if (reason.notifications && reason.engagement) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                NOTIFICATIONS_AND_ENGAGEMENT, REASON_BOUNDARY);
    } else if (reason.durable && reason.engagement) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                DURABLE_AND_ENGAGEMENT, REASON_BOUNDARY);
    } else if (reason.notifications) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                NOTIFICATIONS, REASON_BOUNDARY);
    } else if (reason.durable) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                DURABLE, REASON_BOUNDARY);
    } else if (reason.engagement) {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                ENGAGEMENT, REASON_BOUNDARY);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                                REASON_UNKNOWN, REASON_BOUNDARY);

    }
  }
}

void ImportantSitesUtil::MarkOriginAsImportantForTesting(Profile* profile,
                                                         const GURL& origin) {
  // First get data from site engagement.
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);
  site_engagement_service->ResetScoreForURL(
      origin, SiteEngagementScore::GetMediumEngagementBoundary());
  DCHECK(site_engagement_service->IsEngagementAtLeast(
      origin, SiteEngagementService::ENGAGEMENT_LEVEL_MEDIUM));
}
