// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_STRINGS_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_STRINGS_H_

#include <string>

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace internet_options_strings {

void RegisterLocalizedStrings(base::DictionaryValue* localized_strings);
std::string ActivationStateString(const std::string& activation_state);
std::string RoamingStateString(const std::string& roaming_state);
std::string ProviderTypeString(
    const std::string& provider_type,
    const base::DictionaryValue& provider_properties);
std::string RestrictedStateString(const std::string& connection_state);
std::string NetworkDeviceTypeString(const std::string& network_type);

}  // namespace internet_options_strings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_INTERNET_OPTIONS_HANDLER_STRINGS_H_
