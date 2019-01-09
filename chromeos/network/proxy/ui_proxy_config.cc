// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/ui_proxy_config.h"

#include "base/logging.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "net/proxy_resolution/proxy_config.h"
#include "url/url_constants.h"

namespace chromeos {

UIProxyConfig::UIProxyConfig()
    : mode(MODE_DIRECT), state(ProxyPrefs::CONFIG_UNSET) {}

UIProxyConfig::~UIProxyConfig() = default;

bool UIProxyConfig::FromNetProxyConfig(const net::ProxyConfig& net_config) {
  *this = UIProxyConfig();  // Reset to default.
  const net::ProxyConfig::ProxyRules& rules = net_config.proxy_rules();
  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      if (!net_config.HasAutomaticSettings()) {
        mode = UIProxyConfig::MODE_DIRECT;
      } else if (net_config.auto_detect()) {
        mode = UIProxyConfig::MODE_AUTO_DETECT;
      } else if (net_config.has_pac_url()) {
        mode = UIProxyConfig::MODE_PAC_SCRIPT;
        automatic_proxy.pac_url = net_config.pac_url();
      } else {
        return false;
      }
      return true;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      if (rules.single_proxies.IsEmpty())
        return false;
      mode = MODE_SINGLE_PROXY;
      single_proxy.server = rules.single_proxies.Get();
      return true;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      // Make sure we have valid server for at least one of the protocols.
      if (rules.proxies_for_http.IsEmpty() &&
          rules.proxies_for_https.IsEmpty() &&
          rules.proxies_for_ftp.IsEmpty() && rules.fallback_proxies.IsEmpty()) {
        return false;
      }
      mode = MODE_PROXY_PER_SCHEME;
      if (!rules.proxies_for_http.IsEmpty())
        http_proxy.server = rules.proxies_for_http.Get();
      if (!rules.proxies_for_https.IsEmpty())
        https_proxy.server = rules.proxies_for_https.Get();
      if (!rules.proxies_for_ftp.IsEmpty())
        ftp_proxy.server = rules.proxies_for_ftp.Get();
      if (!rules.fallback_proxies.IsEmpty())
        socks_proxy.server = rules.fallback_proxies.Get();
      return true;
    default:
      NOTREACHED() << "Unrecognized proxy config mode";
      break;
  }
  return false;
}

}  // namespace chromeos
