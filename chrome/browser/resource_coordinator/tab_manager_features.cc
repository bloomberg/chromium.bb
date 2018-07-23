// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/numerics/ranges.h"
#include "chrome/common/chrome_features.h"

namespace {

constexpr char kTabLoadTimeoutInMsParameterName[] = "tabLoadTimeoutInMs";

}  // namespace

namespace features {

// Enables using customized value for tab load timeout. This is used by both
// staggered background tab opening and session restore in finch experiment to
// see what timeout value is better. The default timeout is used when this
// feature is disabled.
const base::Feature kCustomizedTabLoadTimeout{
    "CustomizedTabLoadTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables TabLoader improvements for reducing the overhead of session restores
// involving many many tabs.
const base::Feature kInfiniteSessionRestore{"InfiniteSessionRestore",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables proactive tab freezing and discarding.
const base::Feature kProactiveTabFreezeAndDiscard{
    resource_coordinator::kProactiveTabFreezeAndDiscardFeatureName,
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the site characteristics database.
const base::Feature kSiteCharacteristicsDatabase{
    "SiteCharacteristicsDatabase", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables delaying the navigation of background tabs in order to improve
// foreground tab's user experience.
const base::Feature kStaggeredBackgroundTabOpening{
    "StaggeredBackgroundTabOpening", base::FEATURE_DISABLED_BY_DEFAULT};

// This controls whether we are running experiment with staggered background
// tab opening feature. For control group, this should be disabled. This depends
// on |kStaggeredBackgroundTabOpening| above.
const base::Feature kStaggeredBackgroundTabOpeningExperiment{
    "StaggeredBackgroundTabOpeningExperiment",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables using the Tab Ranker to score tabs for discarding instead of relying
// on last focused time.
const base::Feature kTabRanker{"TabRanker", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace resource_coordinator {

namespace {

// Determines the moderate threshold for tab discarding based on system memory,
// and enforces the constraint that it must be in the interval
// [low_loaded_tab_count, high_loaded_tab_count].
int GetModerateThresholdTabCountBasedOnSystemMemory(
    ProactiveTabFreezeAndDiscardParams* params,
    int memory_in_gb) {
  int moderate_loaded_tab_count_per_gb = base::GetFieldTrialParamByFeatureAsInt(
      features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamParam,
      kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamDefault);

  int moderate_level = moderate_loaded_tab_count_per_gb * memory_in_gb;

  moderate_level =
      base::ClampToRange(moderate_level, params->low_loaded_tab_count,
                         params->high_loaded_tab_count);

  return moderate_level;
}

}  // namespace

const char kProactiveTabFreezeAndDiscardFeatureName[] =
    "ProactiveTabFreezeAndDiscard";

// Field-trial parameter names for proactive tab discarding.
const char kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam[] =
    "ShouldProactivelyDiscard";
const char kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam[] =
    "ShouldPeriodicallyUnfreeze";

// NOTE: This parameter is disabled by default and shouldn't be enabled until
// the privacy review for the UKM associated with it has been approved, see
//  https://docs.google.com/a/google.com/document/d/1BNQ5nLOtPuwP7oxr9r-XKNKr5iObXEiA_69WXAvuYAo/edit?disco=AAAABzM-vE0
//
// TODO(sebmarchand): Remove this comment once the UKM has been approved.
const char
    kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceParam
        [] = "ShouldProtectTabsSharingBrowsingInstance";
const char kProactiveTabFreezeAndDiscard_LowLoadedTabCountParam[] =
    "LowLoadedTabCount";
const char kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamParam[] =
    "ModerateLoadedTabsPerGbRam";
const char kProactiveTabFreezeAndDiscard_HighLoadedTabCountParam[] =
    "HighLoadedTabCount";
const char kProactiveTabFreezeAndDiscard_LowOccludedTimeoutParam[] =
    "LowOccludedTimeoutSeconds";
const char kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutParam[] =
    "ModerateOccludedTimeoutSeconds";
const char kProactiveTabFreezeAndDiscard_HighOccludedTimeoutParam[] =
    "HighOccludedTimeoutSeconds";
const char kProactiveTabFreezeAndDiscard_FreezeTimeoutParam[] = "FreezeTimeout";
const char kProactiveTabFreezeAndDiscard_UnfreezeTimeoutParam[] =
    "UnfreezeTimeout";
const char kProactiveTabFreezeAndDiscard_RefreezeTimeoutParam[] =
    "RefreezeTimeout";

// Field-trial parameter names for the site characteristics database.
const char kSiteCharacteristicsDb_FaviconUpdateObservationWindow[] =
    "FaviconUpdateObservationWindow";
const char kSiteCharacteristicsDb_TitleUpdateObservationWindow[] =
    "TitleUpdateObservationWindow";
const char kSiteCharacteristicsDb_AudioUsageObservationWindow[] =
    "AudioUsageObservationWindow";
const char kSiteCharacteristicsDb_NotificationsUsageObservationWindow[] =
    "NotificationsUsageObservationWindow";
const char kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod[] =
    "TitleOrFaviconChangeGracePeriod";
const char kSiteCharacteristicsDb_AudioUsageGracePeriod[] =
    "AudioUsageGracePeriod";

const char kInfiniteSessionRestore_MinSimultaneousTabLoads[] =
    "MinSimultaneousTabLoads";
const char kInfiniteSessionRestore_MaxSimultaneousTabLoads[] =
    "MaxSimultaneousTabLoads";
const char kInfiniteSessionRestore_CoresPerSimultaneousTabLoad[] =
    "CoresPerSimultaneousTabLoad";
const char kInfiniteSessionRestore_MinTabsToRestore[] = "MinTabsToRestore";
const char kInfiniteSessionRestore_MaxTabsToRestore[] = "MaxTabsToRestore";
const char kInfiniteSessionRestore_MbFreeMemoryPerTabToRestore[] =
    "MbFreeMemoryPerTabToRestore";
const char kInfiniteSessionRestore_MaxTimeSinceLastUseToRestore[] =
    "MaxTimeSinceLastUseToRestore";
const char kInfiniteSessionRestore_MinSiteEngagementToRestore[] =
    "MinSiteEngagementToRestore";

// Default values for ProactiveTabFreezeAndDiscardParams.
const bool kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardDefault =
    false;
const bool kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeDefault =
    false;

// NOTE: This parameter is disabled by default and shouldn't be enabled until
// the privacy review for the UKM associated with it has been approved, see
//  https://docs.google.com/a/google.com/document/d/1BNQ5nLOtPuwP7oxr9r-XKNKr5iObXEiA_69WXAvuYAo/edit?disco=AAAABzM-vE0
//
// TODO(sebmarchand): Remove this comment once the UKM has been approved.
const bool
    kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceDefault =
        false;

// 50% of people cap out at 4 tabs, so for them proactive discarding won't even
// be invoked. See Tabs.MaxTabsInADay.
// TODO(chrisha): This should eventually be informed by the number of tabs
// typically used over a given time horizon (metric being developed).
const uint32_t kProactiveTabFreezeAndDiscard_LowLoadedTabCountDefault = 4;
// Testing in the lab shows that 2GB devices suffer beyond 6 tabs, and 4GB
// devices suffer beyond about 12 tabs. As a very simple first step, we'll aim
// at allowing 3 tabs per GB of RAM on a system before proactive discarding
// kicks in. This is a system resource dependent max, which is combined with the
// DefaultMaxLoadedTabCount to determine the max on a system.
const uint32_t kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamDefault =
    3;
// 99.9% of people cap out with fewer than this number, so only 0.1% of the
// population should ever encounter proactive discarding based on this cap.
const uint32_t kProactiveTabFreezeAndDiscard_HighLoadedTabCountDefault = 100;
// Current discarding uses 10 minutes as a minimum cap. This uses exponentially
// increasing timeouts beyond that.
const base::TimeDelta kProactiveTabFreezeAndDiscard_LowOccludedTimeoutDefault =
    base::TimeDelta::FromHours(6);
const base::TimeDelta
    kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutDefault =
        base::TimeDelta::FromHours(1);
const base::TimeDelta kProactiveTabFreezeAndDiscard_HighOccludedTimeoutDefault =
    base::TimeDelta::FromMinutes(10);
const base::TimeDelta kProactiveTabFreezeAndDiscard_FreezeTimeoutDefault =
    base::TimeDelta::FromMinutes(10);
const base::TimeDelta kProactiveTabFreezeAndDiscard_UnfreezeTimeoutDefault =
    base::TimeDelta::FromMinutes(15);
const base::TimeDelta kProactiveTabFreezeAndDiscard_RefreezeTimeoutDefault =
    base::TimeDelta::FromSeconds(10);

// Default values for SiteCharacteristicsDatabaseParams.
//
// Observations windows have a default value of 2 hours, 95% of backgrounded
// tabs don't use any of these features in this time window.
const base::TimeDelta
    kSiteCharacteristicsDb_FaviconUpdateObservationWindow_Default =
        base::TimeDelta::FromHours(2);
const base::TimeDelta
    kSiteCharacteristicsDb_TitleUpdateObservationWindow_Default =
        base::TimeDelta::FromHours(2);
const base::TimeDelta
    kSiteCharacteristicsDb_AudioUsageObservationWindow_Default =
        base::TimeDelta::FromHours(2);
const base::TimeDelta
    kSiteCharacteristicsDb_NotificationsUsageObservationWindow_Default =
        base::TimeDelta::FromHours(2);

// TODO(sebmarchand): Get some real-world data and choose an appropriate value
// here.
const base::TimeDelta
    kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod_Default =
        base::TimeDelta::FromSeconds(20);
const base::TimeDelta kSiteCharacteristicsDb_AudioUsageGracePeriod_Default =
    base::TimeDelta::FromSeconds(10);

// Default values for infinite session restore feature. Many of these are taken
// from thin air, but others are motivated by existing metrics.
const uint32_t kInfiniteSessionRestore_MinSimultaneousTabLoadsDefault = 1;
const uint32_t kInfiniteSessionRestore_MaxSimultaneousTabLoadsDefault = 4;
const uint32_t kInfiniteSessionRestore_CoresPerSimultaneousTabLoadDefault = 2;
const uint32_t kInfiniteSessionRestore_MinTabsToRestoreDefault = 4;
const uint32_t kInfiniteSessionRestore_MaxTabsToRestoreDefault = 20;
// This is the 75th percentile of Memory.Renderer.PrivateMemoryFootprint.
const uint32_t kInfiniteSessionRestore_MbFreeMemoryPerTabToRestoreDefault = 150;
// This is the 75th percentile of SessionRestore.RestoredTab.TimeSinceActive.
const base::TimeDelta
    kInfiniteSessionRestore_MaxTimeSinceLastUseToRestoreDefault =
        base::TimeDelta::FromHours(6);
// Taken from an informal survey of Googlers on min engagement of things they
// think *must* load. Note that about 25% of session-restore tabs fall above
// this threshold (see SessionRestore.RestoredTab.SiteEngagementScore).
const uint32_t kInfiniteSessionRestore_MinSiteEngagementToRestoreDefault = 15;

ProactiveTabFreezeAndDiscardParams::ProactiveTabFreezeAndDiscardParams() =
    default;
ProactiveTabFreezeAndDiscardParams::ProactiveTabFreezeAndDiscardParams(
    const ProactiveTabFreezeAndDiscardParams& rhs) = default;

SiteCharacteristicsDatabaseParams::SiteCharacteristicsDatabaseParams() =
    default;
SiteCharacteristicsDatabaseParams::SiteCharacteristicsDatabaseParams(
    const SiteCharacteristicsDatabaseParams& rhs) = default;

InfiniteSessionRestoreParams::InfiniteSessionRestoreParams() = default;
InfiniteSessionRestoreParams::InfiniteSessionRestoreParams(
    const InfiniteSessionRestoreParams& rhs) = default;

ProactiveTabFreezeAndDiscardParams GetProactiveTabFreezeAndDiscardParams(
    int memory_in_gb) {
  ProactiveTabFreezeAndDiscardParams params = {};

  params.should_proactively_discard = base::GetFieldTrialParamByFeatureAsBool(
      features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardParam,
      kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardDefault);

  params.should_periodically_unfreeze = base::GetFieldTrialParamByFeatureAsBool(
      features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeParam,
      kProactiveTabFreezeAndDiscard_ShouldPeriodicallyUnfreezeDefault);

  params.should_protect_tabs_sharing_browsing_instance =
      base::GetFieldTrialParamByFeatureAsBool(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceParam,
          kProactiveTabFreezeAndDiscard_ShouldProtectTabsSharingBrowsingInstanceDefault);

  params.low_loaded_tab_count = base::GetFieldTrialParamByFeatureAsInt(
      features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_LowLoadedTabCountParam,
      kProactiveTabFreezeAndDiscard_LowLoadedTabCountDefault);

  params.high_loaded_tab_count = base::GetFieldTrialParamByFeatureAsInt(
      features::kProactiveTabFreezeAndDiscard,
      kProactiveTabFreezeAndDiscard_HighLoadedTabCountParam,
      kProactiveTabFreezeAndDiscard_HighLoadedTabCountDefault);

  // |moderate_loaded_tab_count| determined after |high_loaded_tab_count| so it
  // can be enforced that it is lower than |high_loaded_tab_count|.
  params.moderate_loaded_tab_count =
      GetModerateThresholdTabCountBasedOnSystemMemory(&params, memory_in_gb);

  params.low_occluded_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_LowOccludedTimeoutParam,
          kProactiveTabFreezeAndDiscard_LowOccludedTimeoutDefault.InSeconds()));

  params.moderate_occluded_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutParam,
          kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutDefault
              .InSeconds()));

  params.high_occluded_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_HighOccludedTimeoutParam,
          kProactiveTabFreezeAndDiscard_HighOccludedTimeoutDefault
              .InSeconds()));

  params.freeze_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_FreezeTimeoutParam,
          kProactiveTabFreezeAndDiscard_FreezeTimeoutDefault.InSeconds()));

  params.unfreeze_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_UnfreezeTimeoutParam,
          kProactiveTabFreezeAndDiscard_UnfreezeTimeoutDefault.InSeconds()));

  params.refreeze_timeout =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kProactiveTabFreezeAndDiscard,
          kProactiveTabFreezeAndDiscard_RefreezeTimeoutParam,
          kProactiveTabFreezeAndDiscard_RefreezeTimeoutDefault.InSeconds()));

  return params;
}

const ProactiveTabFreezeAndDiscardParams&
GetStaticProactiveTabFreezeAndDiscardParams() {
  static base::NoDestructor<ProactiveTabFreezeAndDiscardParams> params(
      GetProactiveTabFreezeAndDiscardParams());
  return *params;
}

ProactiveTabFreezeAndDiscardParams*
GetMutableStaticProactiveTabFreezeAndDiscardParamsForTesting() {
  return const_cast<ProactiveTabFreezeAndDiscardParams*>(
      &GetStaticProactiveTabFreezeAndDiscardParams());
}

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout) {
  int timeout_in_ms = base::GetFieldTrialParamByFeatureAsInt(
      features::kCustomizedTabLoadTimeout, kTabLoadTimeoutInMsParameterName,
      default_timeout.InMilliseconds());

  if (timeout_in_ms <= 0)
    return default_timeout;

  return base::TimeDelta::FromMilliseconds(timeout_in_ms);
}

SiteCharacteristicsDatabaseParams GetSiteCharacteristicsDatabaseParams() {
  SiteCharacteristicsDatabaseParams params = {};

  params.favicon_update_observation_window =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_FaviconUpdateObservationWindow,
          kSiteCharacteristicsDb_FaviconUpdateObservationWindow_Default
              .InSeconds()));

  params.title_update_observation_window =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_TitleUpdateObservationWindow,
          kSiteCharacteristicsDb_TitleUpdateObservationWindow_Default
              .InSeconds()));

  params.audio_usage_observation_window =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_AudioUsageObservationWindow,
          kSiteCharacteristicsDb_AudioUsageObservationWindow_Default
              .InSeconds()));

  params.notifications_usage_observation_window =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_NotificationsUsageObservationWindow,
          kSiteCharacteristicsDb_NotificationsUsageObservationWindow_Default
              .InSeconds()));

  params.title_or_favicon_change_grace_period =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod,
          kSiteCharacteristicsDb_TitleOrFaviconChangeGracePeriod_Default
              .InSeconds()));

  params.audio_usage_grace_period =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kSiteCharacteristicsDatabase,
          kSiteCharacteristicsDb_AudioUsageGracePeriod,
          kSiteCharacteristicsDb_AudioUsageGracePeriod_Default.InSeconds()));

  return params;
}

const SiteCharacteristicsDatabaseParams&
GetStaticSiteCharacteristicsDatabaseParams() {
  static base::NoDestructor<SiteCharacteristicsDatabaseParams> params(
      GetSiteCharacteristicsDatabaseParams());
  return *params;
}

InfiniteSessionRestoreParams GetInfiniteSessionRestoreParams() {
  InfiniteSessionRestoreParams params = {};

  params.min_simultaneous_tab_loads = base::GetFieldTrialParamByFeatureAsInt(
      features::kInfiniteSessionRestore,
      kInfiniteSessionRestore_MinSimultaneousTabLoads,
      kInfiniteSessionRestore_MinSimultaneousTabLoadsDefault);
  params.max_simultaneous_tab_loads = base::GetFieldTrialParamByFeatureAsInt(
      features::kInfiniteSessionRestore,
      kInfiniteSessionRestore_MaxSimultaneousTabLoads,
      kInfiniteSessionRestore_MaxSimultaneousTabLoadsDefault);
  params.cores_per_simultaneous_tab_load =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kInfiniteSessionRestore,
          kInfiniteSessionRestore_CoresPerSimultaneousTabLoad,
          kInfiniteSessionRestore_CoresPerSimultaneousTabLoadDefault);
  params.min_tabs_to_restore = base::GetFieldTrialParamByFeatureAsInt(
      features::kInfiniteSessionRestore,
      kInfiniteSessionRestore_MinTabsToRestore,
      kInfiniteSessionRestore_MinTabsToRestoreDefault);
  params.max_tabs_to_restore = base::GetFieldTrialParamByFeatureAsInt(
      features::kInfiniteSessionRestore,
      kInfiniteSessionRestore_MaxTabsToRestore,
      kInfiniteSessionRestore_MaxTabsToRestoreDefault);
  params.mb_free_memory_per_tab_to_restore =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kInfiniteSessionRestore,
          kInfiniteSessionRestore_MbFreeMemoryPerTabToRestore,
          kInfiniteSessionRestore_MbFreeMemoryPerTabToRestoreDefault);
  params.max_time_since_last_use_to_restore =
      base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kInfiniteSessionRestore,
          kInfiniteSessionRestore_MaxTimeSinceLastUseToRestore,
          kInfiniteSessionRestore_MaxTimeSinceLastUseToRestoreDefault
              .InSeconds()));
  params.min_site_engagement_to_restore =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kInfiniteSessionRestore,
          kInfiniteSessionRestore_MinSiteEngagementToRestore,
          kInfiniteSessionRestore_MinSiteEngagementToRestoreDefault);

  return params;
}

}  // namespace resource_coordinator
