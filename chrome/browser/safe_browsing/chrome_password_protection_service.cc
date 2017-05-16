// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing_db/database_manager.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;

}  // namespace

ChromePasswordProtectionService::ChromePasswordProtectionService(
    SafeBrowsingService* sb_service,
    Profile* profile)
    : PasswordProtectionService(
          sb_service->database_manager(),
          sb_service->url_request_context(),
          HistoryServiceFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          HostContentSettingsMapFactory::GetForProfile(profile)),
      profile_(profile),
      navigation_observer_manager_(sb_service->navigation_observer_manager()) {
  DCHECK(profile_);
}

ChromePasswordProtectionService::~ChromePasswordProtectionService() {
  if (content_settings()) {
    CleanUpExpiredVerdicts();
    UMA_HISTOGRAM_COUNTS_1000(
        "PasswordProtection.NumberOfCachedVerdictBeforeShutdown",
        GetStoredVerdictCount());
  }
}

void ChromePasswordProtectionService::FillReferrerChain(
    const GURL& event_url,
    int event_tab_id,
    LoginReputationClientRequest::Frame* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByEventURL(
          event_url, event_tab_id, kPasswordEventAttributionUserGestureLimit,
          frame->mutable_referrer_chain());
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.PasswordEventAttribution",
      frame->referrer_chain().size());
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.PasswordEventAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  return IsExtendedReportingEnabled(*profile_->GetPrefs());
}

bool ChromePasswordProtectionService::IsIncognito() {
  DCHECK(profile_);
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    const base::Feature& feature,
    RequestOutcome* reason) {
  DCHECK(feature.name == kProtectedPasswordEntryPinging.name ||
         feature.name == kPasswordFieldOnFocusPinging.name);
  if (!base::FeatureList::IsEnabled(feature)) {
    *reason = DISABLED_DUE_TO_FEATURE_DISABLED;
    return false;
  }

  bool allowed_incognito =
      base::GetFieldTrialParamByFeatureAsBool(feature, "incognito", false);
  if (IsIncognito() && !allowed_incognito) {
    *reason = DISABLED_DUE_TO_INCOGNITO;
    return false;
  }

  bool allowed_all_population =
      base::GetFieldTrialParamByFeatureAsBool(feature, "all_population", false);
  if (!allowed_all_population) {
    bool allowed_extended_reporting = base::GetFieldTrialParamByFeatureAsBool(
        feature, "extended_reporting", false);
    if (IsExtendedReporting() && allowed_extended_reporting)
      return true;  // Ping enabled because this user opted in extended
                    // reporting.

    bool allowed_history_sync =
        base::GetFieldTrialParamByFeatureAsBool(feature, "history_sync", false);
    if (IsHistorySyncEnabled() && allowed_history_sync)
      return true;

    *reason = DISABLED_DUE_TO_USER_POPULATION;
  }

  return allowed_all_population;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return sync && sync->IsSyncActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

ChromePasswordProtectionService::ChromePasswordProtectionService()
    : PasswordProtectionService(nullptr, nullptr, nullptr, nullptr) {}
}  // namespace safe_browsing
