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
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  ObserveServicePrefChange(Service::kMetricsReporting,
                           metrics::prefs::kMetricsReportingEnabled,
                           pref_service_);
  ObserveServicePrefChange(Service::kNetworkPrediction,
                           prefs::kNetworkPredictionEnabled, pref_service_);
  ObserveServicePrefChange(Service::kSearchSuggest,
                           prefs::kSearchSuggestEnabled, pref_service_);
}

UnifiedConsentServiceClientImpl::ServiceState
UnifiedConsentServiceClientImpl::GetServiceState(Service service) {
  bool enabled;
  switch (service) {
    case Service::kMetricsReporting: {
      BooleanPrefMember metrics_pref;
      metrics_pref.Init(metrics::prefs::kMetricsReportingEnabled,
                        pref_service_);
      enabled = metrics_pref.GetValue();
      break;
    }
    case Service::kNetworkPrediction: {
      BooleanPrefMember network_pref;
      network_pref.Init(prefs::kNetworkPredictionEnabled, pref_service_);
      enabled = network_pref.GetValue();
      break;
    }
    case Service::kSearchSuggest:
      enabled = pref_service_->GetBoolean(prefs::kSearchSuggestEnabled);
      break;
    case Service::kAlternateErrorPages:
    case Service::kSafeBrowsing:
    case Service::kSafeBrowsingExtendedReporting:
    case Service::kSpellCheck:
      return ServiceState::kNotSupported;
  }
  return enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
}

void UnifiedConsentServiceClientImpl::SetServiceEnabled(Service service,
                                                        bool enabled) {
  switch (service) {
    case Service::kMetricsReporting: {
      BooleanPrefMember metrics_pref;
      metrics_pref.Init(metrics::prefs::kMetricsReportingEnabled,
                        pref_service_);
      metrics_pref.SetValue(enabled);
      BooleanPrefMember metrics_wifi_pref;
      metrics_wifi_pref.Init(prefs::kMetricsReportingWifiOnly, pref_service_);
      metrics_wifi_pref.SetValue(enabled);
      break;
    }
    case Service::kNetworkPrediction: {
      BooleanPrefMember network_pref;
      network_pref.Init(prefs::kNetworkPredictionEnabled, pref_service_);
      network_pref.SetValue(enabled);
      BooleanPrefMember network_wifi_pref;
      network_wifi_pref.Init(prefs::kNetworkPredictionWifiOnly, pref_service_);
      network_wifi_pref.SetValue(enabled);
      break;
    }
    case Service::kSearchSuggest:
      pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
      break;
    case Service::kAlternateErrorPages:
    case Service::kSafeBrowsing:
    case Service::kSafeBrowsingExtendedReporting:
    case Service::kSpellCheck:
      NOTIMPLEMENTED() << "Feature not available on iOS";
      break;
  }
}
