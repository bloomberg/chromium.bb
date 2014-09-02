// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/prediction_options.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

namespace chrome_browser_net {

namespace {

// Since looking up preferences and current network connection are presumably
// both cheap, we do not cache them here.
bool CanPrefetchAndPrerender(int network_prediction_options) {
  switch (network_prediction_options) {
    case NETWORK_PREDICTION_ALWAYS:
      return true;
    case NETWORK_PREDICTION_NEVER:
      return false;
    default:
      DCHECK_EQ(NETWORK_PREDICTION_WIFI_ONLY, network_prediction_options);
      return !net::NetworkChangeNotifier::IsConnectionCellular(
                 net::NetworkChangeNotifier::GetConnectionType());
  }
}

bool CanPreresolveAndPreconnect(int network_prediction_options) {
  // DNS preresolution and TCP preconnect are performed even on cellular
  // networks if the user setting is WIFI_ONLY.
  return network_prediction_options != NETWORK_PREDICTION_NEVER;
}

}  // namespace

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kNetworkPredictionOptions,
      NETWORK_PREDICTION_DEFAULT,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void MigrateNetworkPredictionUserPrefs(PrefService* pref_service) {
  // Nothing to do if the user or this migration code has already set the new
  // preference.
  if (pref_service->GetUserPrefValue(prefs::kNetworkPredictionOptions))
    return;

  // Nothing to do if the user has not set the old preference.
  const base::Value* network_prediction_enabled =
      pref_service->GetUserPrefValue(prefs::kNetworkPredictionEnabled);
  if (!network_prediction_enabled)
    return;

  bool value = false;
  if (network_prediction_enabled->GetAsBoolean(&value)) {
    pref_service->SetInteger(
        prefs::kNetworkPredictionOptions,
        value ? NETWORK_PREDICTION_WIFI_ONLY : NETWORK_PREDICTION_NEVER);
  }
}

bool CanPrefetchAndPrerenderIO(ProfileIOData* profile_io_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(profile_io_data);
  return CanPrefetchAndPrerender(
      profile_io_data->network_prediction_options()->GetValue());
}

bool CanPrefetchAndPrerenderUI(PrefService* prefs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(prefs);
  return CanPrefetchAndPrerender(
      prefs->GetInteger(prefs::kNetworkPredictionOptions));
}

bool CanPreresolveAndPreconnectIO(ProfileIOData* profile_io_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(profile_io_data);
  return CanPreresolveAndPreconnect(
      profile_io_data->network_prediction_options()->GetValue());
}

bool CanPreresolveAndPreconnectUI(PrefService* prefs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(prefs);
  return CanPreresolveAndPreconnect(
      prefs->GetInteger(prefs::kNetworkPredictionOptions));
}

}  // namespace chrome_browser_net
