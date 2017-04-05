// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {
// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;
}

const base::Feature kPasswordProtectionPingOnly{
    "PasswordProtectionPingOnly", base::FEATURE_DISABLED_BY_DEFAULT};

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
  UMA_HISTOGRAM_COUNTS_1000(
      "PasswordProtection.NumberOfCachedVerdictBeforeShutdown",
      GetStoredVerdictCount());
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

bool ChromePasswordProtectionService::IsPingingEnabled() {
  return base::FeatureList::IsEnabled(kPasswordProtectionPingOnly);
}

}  // namespace safe_browsing
