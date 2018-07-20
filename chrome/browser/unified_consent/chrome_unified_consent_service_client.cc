// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"

#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"

ChromeUnifiedConsentServiceClient::ChromeUnifiedConsentServiceClient(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void ChromeUnifiedConsentServiceClient::SetAlternateErrorPagesEnabled(
    bool enabled) {
  pref_service_->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
}

void ChromeUnifiedConsentServiceClient::SetMetricsReportingEnabled(
    bool enabled) {
  ChangeMetricsReportingState(enabled);
}

void ChromeUnifiedConsentServiceClient::SetSearchSuggestEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
}

void ChromeUnifiedConsentServiceClient::SetSafeBrowsingEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, enabled);
}

void ChromeUnifiedConsentServiceClient::SetSafeBrowsingExtendedReportingEnabled(
    bool enabled) {
  safe_browsing::SetExtendedReportingPref(pref_service_, enabled);
}

void ChromeUnifiedConsentServiceClient::SetNetworkPredictionEnabled(
    bool enabled) {
  pref_service_->SetInteger(prefs::kNetworkPredictionOptions,
                            enabled
                                ? chrome_browser_net::NETWORK_PREDICTION_DEFAULT
                                : chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

void ChromeUnifiedConsentServiceClient::SetSpellCheckEnabled(bool enabled) {
  pref_service_->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService,
                            enabled);
}
