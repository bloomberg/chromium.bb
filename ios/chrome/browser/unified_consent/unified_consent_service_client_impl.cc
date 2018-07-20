// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/unified_consent_service_client_impl.h"

#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/pref_names.h"

UnifiedConsentServiceClientImpl::UnifiedConsentServiceClientImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void UnifiedConsentServiceClientImpl::SetAlternateErrorPagesEnabled(
    bool enabled) {
  // Feature not available on iOS.
  NOTIMPLEMENTED();
}

void UnifiedConsentServiceClientImpl::SetMetricsReportingEnabled(bool enabled) {
  BooleanPrefMember basePreference;
  basePreference.Init(metrics::prefs::kMetricsReportingEnabled, pref_service_);
  basePreference.SetValue(enabled);
  BooleanPrefMember wifiPreference;
  wifiPreference.Init(prefs::kMetricsReportingWifiOnly, pref_service_);
  wifiPreference.SetValue(enabled);
}

void UnifiedConsentServiceClientImpl::SetSearchSuggestEnabled(bool enabled) {
  pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
}

void UnifiedConsentServiceClientImpl::SetSafeBrowsingEnabled(bool enabled) {
  // Feature not available on iOS.
  NOTIMPLEMENTED();
}

void UnifiedConsentServiceClientImpl::SetSafeBrowsingExtendedReportingEnabled(
    bool enabled) {
  // Feature not available on iOS.
  NOTIMPLEMENTED();
}

void UnifiedConsentServiceClientImpl::SetNetworkPredictionEnabled(
    bool enabled) {
  BooleanPrefMember basePreference;
  basePreference.Init(prefs::kNetworkPredictionEnabled, pref_service_);
  basePreference.SetValue(enabled);
  BooleanPrefMember wifiPreference;
  wifiPreference.Init(prefs::kNetworkPredictionWifiOnly, pref_service_);
  wifiPreference.SetValue(enabled);
}

void UnifiedConsentServiceClientImpl::SetSpellCheckEnabled(bool enabled) {
  // Feature not available on iOS.
  NOTIMPLEMENTED();
}
