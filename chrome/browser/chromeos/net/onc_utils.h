// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

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

}  // namespace onc
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
