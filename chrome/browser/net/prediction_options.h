// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_
#define CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class ProfileIOData;

namespace chrome_browser_net {

// Enum describing when to allow network predictions based on connection type.
// The numerical value is stored in the prefs file, therefore the same enum
// with the same order must be used by the platform-dependent components.
// TODO(bnc): implement as per crbug.com/334602.
// NETWORK_PREDICTION_UNSET means that the old preferences,
// kNetworkPredictionEnabled and kAllowPrerender, should be observed.
enum NetworkPredictionOptions {
  NETWORK_PREDICTION_ALWAYS,
  NETWORK_PREDICTION_WIFI_ONLY,
  NETWORK_PREDICTION_NEVER,
  NETWORK_PREDICTION_UNSET,
};

void RegisterPredictionOptionsProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Migrate prefs::kNetworkPredictionEnabled user setting (if exists) to
// prefs::kNetworkPredictionOptions (if does not have user setting).
void MigrateNetworkPredictionUserPrefs(PrefService* pref_service);

// The following two global functions determine whether predictive network
// actions are enabled, based on preferences and network type.

// To be executed on the IO thread only.
bool CanPredictNetworkActionsIO(ProfileIOData* profile_io_data);

// To be executed on the UI thread only.
bool CanPredictNetworkActionsUI(PrefService* prefs);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTION_OPTIONS_H_
