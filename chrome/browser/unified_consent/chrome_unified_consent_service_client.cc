// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/contextual_search/buildflags.h"
#include "components/contextual_search/core/browser/contextual_search_preference.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"

ChromeUnifiedConsentServiceClient::ChromeUnifiedConsentServiceClient(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  ObserveServicePrefChange(Service::kSafeBrowsingExtendedReporting,
                           prefs::kSafeBrowsingScoutReportingEnabled,
                           pref_service_);
  ObserveServicePrefChange(Service::kSpellCheck,
                           spellcheck::prefs::kSpellCheckUseSpellingService,
                           pref_service_);
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
  ObserveServicePrefChange(Service::kContextualSearch,
                           contextual_search::GetPrefName(), pref_service_);
#endif
}

ChromeUnifiedConsentServiceClient::~ChromeUnifiedConsentServiceClient() {}

ChromeUnifiedConsentServiceClient::ServiceState
ChromeUnifiedConsentServiceClient::GetServiceState(Service service) {
  bool enabled;
  switch (service) {
    case Service::kSafeBrowsingExtendedReporting:
      enabled = safe_browsing::IsExtendedReportingEnabled(*pref_service_);
      break;
    case Service::kSpellCheck:
      enabled = pref_service_->GetBoolean(
          spellcheck::prefs::kSpellCheckUseSpellingService);
      break;
    case Service::kContextualSearch:
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
      enabled = contextual_search::IsEnabled(*pref_service_);
#else
      return ServiceState::kNotSupported;
#endif
      break;
    case Service::kAlternateErrorPages:
    case Service::kMetricsReporting:
    case Service::kNetworkPrediction:
    case Service::kSafeBrowsing:
    case Service::kSearchSuggest:
      return ServiceState::kNotSupported;
  }
  return enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
}

void ChromeUnifiedConsentServiceClient::SetServiceEnabled(Service service,
                                                          bool enabled) {
  if (GetServiceState(service) == ServiceState::kNotSupported)
    return;

  switch (service) {
    case Service::kSafeBrowsingExtendedReporting:
      safe_browsing::SetExtendedReportingPref(pref_service_, enabled);
      break;
    case Service::kSpellCheck:
      pref_service_->SetBoolean(
          spellcheck::prefs::kSpellCheckUseSpellingService, enabled);
      break;
    case Service::kContextualSearch: {
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
      contextual_search::ContextualSearchPreference* contextual_search_pref =
          contextual_search::ContextualSearchPreference::GetInstance();
      if (enabled)
        contextual_search_pref->EnableIfUndecided(pref_service_);
      else
        contextual_search_pref->SetPref(pref_service_, false);
#else
      NOTREACHED();
#endif
      break;
    }
    case Service::kAlternateErrorPages:
    case Service::kMetricsReporting:
    case Service::kNetworkPrediction:
    case Service::kSafeBrowsing:
    case Service::kSearchSuggest:
      NOTREACHED();
  }
}
