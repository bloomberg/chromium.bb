// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_

#include "base/memory/scoped_ptr.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class NetworkUIData;

namespace onc {

// Translates |onc_proxy_settings|, which has to be a valid ONC ProxySettings
// dictionary, to a ProxyConfig dictionary (see
// chrome/browser/prefs/proxy_config_dictionary.h).
//
// This function is used to translate ONC ProxySettings to the "ProxyConfig"
// field of the Shill configuration.
scoped_ptr<base::DictionaryValue> ConvertOncProxySettingsToProxyConfig(
    const base::DictionaryValue& onc_proxy_settings);

// Creates a NetworkUIData object from |onc_network|, which has to be a valid
// ONC NetworkConfiguration dictionary.
//
// This function is used to create the "UIData" field of the Shill
// configuration.
scoped_ptr<NetworkUIData> CreateUIData(
    ONCSource onc_source,
    const base::DictionaryValue& onc_network);

}  // namespace onc
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_ONC_UTILS_H_
