// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_eviction_policy.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

namespace {

const int kExpectedEngagementSites = 200;

// Gets the quota that an origin deserves based on its site engagement.
int64 GetSoftQuotaForOrigin(const GURL& origin,
                            int score,
                            int total_engagement_points,
                            int64 global_quota) {
  double quota_per_point =
      global_quota /
      std::max(kExpectedEngagementSites * SiteEngagementScore::kMaxPoints,
               static_cast<double>(total_engagement_points));

  return score * quota_per_point;
}

GURL DoCalculateEvictionOrigin(
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    SiteEngagementScoreProvider* score_provider,
    const std::set<GURL>& exceptions,
    const std::map<GURL, int64>& usage_map,
    int64 global_quota) {
  // TODO(calamity): Integrate storage access frequency as an input to this
  // heuristic.

  // This heuristic is intended to optimize for two criteria:
  // - evict the site that the user cares about least
  // - evict the least number of sites to get under the quota limit
  //
  // The heuristic for deciding the next eviction origin calculates a soft
  // quota for each origin which is the amount the origin should be allowed to
  // use based on its engagement and the global quota. The origin that most
  // exceeds its soft quota is chosen.
  GURL origin_to_evict;
  int64 max_overuse = std::numeric_limits<int64>::min();
  int total_engagement_points = score_provider->GetTotalEngagementPoints();

  for (const auto& usage : usage_map) {
    GURL origin = usage.first;
    if (special_storage_policy &&
        (special_storage_policy->IsStorageUnlimited(origin) ||
         special_storage_policy->IsStorageDurable(origin))) {
      continue;
    }

    // |overuse| can be negative if the soft quota exceeds the usage.
    int64 overuse = usage.second - GetSoftQuotaForOrigin(
                                       origin, score_provider->GetScore(origin),
                                       total_engagement_points, global_quota);
    if (overuse > max_overuse && !ContainsKey(exceptions, origin)) {
      max_overuse = overuse;
      origin_to_evict = origin;
    }
  }

  return origin_to_evict;
}

GURL GetSiteEngagementEvictionOriginOnUIThread(
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    content::BrowserContext* browser_context,
    const std::set<GURL>& exceptions,
    const std::map<GURL, int64>& usage_map,
    int64 global_quota) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  SiteEngagementScoreProvider* score_provider =
      g_browser_process->profile_manager()->IsValidProfile(profile)
          ? SiteEngagementService::Get(profile)
          : nullptr;

  if (!score_provider)
    return GURL();

  return DoCalculateEvictionOrigin(special_storage_policy, score_provider,
                                   exceptions, usage_map, global_quota);
}

}  // namespace

// static
bool SiteEngagementEvictionPolicy::IsEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSiteEngagementEvictionPolicy)) {
    return true;
  }

  const std::string group_name = base::FieldTrialList::FindFullName(
      SiteEngagementService::kEngagementParams);
  return base::StartsWith(group_name, "StorageEvictionEnabled",
                          base::CompareCase::SENSITIVE);
}

SiteEngagementEvictionPolicy::SiteEngagementEvictionPolicy(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

SiteEngagementEvictionPolicy::~SiteEngagementEvictionPolicy() {}

void SiteEngagementEvictionPolicy::GetEvictionOrigin(
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const std::set<GURL>& exceptions,
    const std::map<GURL, int64>& usage_map,
    int64 global_quota,
    const storage::GetOriginCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&GetSiteEngagementEvictionOriginOnUIThread,
                 special_storage_policy, browser_context_, exceptions,
                 usage_map, global_quota),
      callback);
}

// static
GURL SiteEngagementEvictionPolicy::CalculateEvictionOriginForTests(
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    SiteEngagementScoreProvider* score_provider,
    const std::set<GURL>& exceptions,
    const std::map<GURL, int64>& usage_map,
    int64 global_quota) {
  return DoCalculateEvictionOrigin(special_storage_policy, score_provider,
                                   exceptions, usage_map, global_quota);
}
