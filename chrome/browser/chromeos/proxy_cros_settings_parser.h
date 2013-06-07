// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PARSER_H_

#include <string>

namespace base {
class Value;
}

namespace chromeos {

class UIProxyConfigService;

extern const char kProxyPacUrl[];
extern const char kProxySingleHttp[];
extern const char kProxySingleHttpPort[];
extern const char kProxyHttpUrl[];
extern const char kProxyHttpPort[];
extern const char kProxyHttpsUrl[];
extern const char kProxyHttpsPort[];
extern const char kProxyType[];
extern const char kProxySingle[];
extern const char kProxyFtpUrl[];
extern const char kProxyFtpPort[];
extern const char kProxySocks[];
extern const char kProxySocksPort[];
extern const char kProxyIgnoreList[];
extern const char kProxyUsePacUrl[];

extern const char* const kProxySettings[];
extern const size_t kProxySettingsCount;

// This namespace defines helper functions for setting/getting Proxy settings.
namespace proxy_cros_settings_parser {

// Returns true if the supplied |path| is a proxy preference name.
bool IsProxyPref(const std::string& path);

// Sets a value in the current proxy configuration on the specified profile.
void SetProxyPrefValue(const std::string& path,
                       const base::Value* in_value,
                       UIProxyConfigService* config_service);

// Gets a value from the current proxy configuration on the specified profile.
bool GetProxyPrefValue(const UIProxyConfigService& config_service,
                       const std::string& path,
                       base::Value** out_value);

}  // namespace proxy_cros_settings_parser

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CROS_SETTINGS_PARSER_H_
