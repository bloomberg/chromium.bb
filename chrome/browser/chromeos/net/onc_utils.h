// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/onc/onc_constants.h"

class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class FavoriteState;
class NetworkState;
class User;

namespace onc {

// Translates |onc_proxy_settings|, which has to be a valid ONC ProxySettings
// dictionary, to a ProxyConfig dictionary (see
// chrome/browser/prefs/proxy_config_dictionary.h).
//
// This function is used to translate ONC ProxySettings to the "ProxyConfig"
// field of the Shill configuration.
scoped_ptr<base::DictionaryValue> ConvertOncProxySettingsToProxyConfig(
    const base::DictionaryValue& onc_proxy_settings);

// Replaces string placeholders in |network_configs|, which must be a list of
// ONC NetworkConfigurations. Currently only user name placeholders are
// implemented, which are replaced by attributes of the logged-in user with
// |hashed_username|.
void ExpandStringPlaceholdersInNetworksForUser(
    const chromeos::User* user,
    base::ListValue* network_configs);

void ImportNetworksForUser(const chromeos::User* user,
                           const base::ListValue& network_configs,
                           std::string* error);

// Looks up the policy for |guid| for the current active user and sets
// |onc_source| accordingly.
const base::DictionaryValue* FindPolicyForActiveUser(
    const std::string& guid,
    ::onc::ONCSource* onc_source);

// Returns the effective (user or device) policy for network |favorite|. Both
// |profile_prefs| and |local_state_prefs| might be NULL. Returns NULL if no
// applicable policy is found. Sets |onc_source| accordingly.
const base::DictionaryValue* GetPolicyForFavoriteNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const FavoriteState& favorite,
    ::onc::ONCSource* onc_source);

// Convenience function to check only whether a policy for a network exists.
bool HasPolicyForFavoriteNetwork(const PrefService* profile_prefs,
                                 const PrefService* local_state_prefs,
                                 const FavoriteState& network);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
