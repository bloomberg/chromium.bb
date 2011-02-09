// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_proxy_api.h"

#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"

namespace {

// The scheme for which to use a manually specified proxy, not of the proxy URI
// itself.
enum {
  SCHEME_ALL = 0,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FTP,
  SCHEME_SOCKS,
  SCHEME_MAX = SCHEME_SOCKS  // Keep this value up to date.
};

// The names of the JavaScript properties to extract from the proxy_rules.
// These must be kept in sync with the SCHEME_* constants.
const char* field_name[] = { "singleProxy",
                             "proxyForHttp",
                             "proxyForHttps",
                             "proxyForFtp",
                             "socksProxy" };

// The names of the schemes to be used to build the preference value string
// for manual proxy settings.  These must be kept in sync with the SCHEME_*
// constants.
const char* scheme_name[] = { "*error*",
                              "http",
                              "https",
                              "ftp",
                              "socks" };

COMPILE_ASSERT(SCHEME_MAX == SCHEME_SOCKS, SCHEME_MAX_must_equal_SCHEME_SOCKS);
COMPILE_ASSERT(arraysize(field_name) == SCHEME_MAX + 1,
               field_name_array_is_wrong_size);
COMPILE_ASSERT(arraysize(scheme_name) == SCHEME_MAX + 1,
               scheme_name_array_is_wrong_size);
COMPILE_ASSERT(SCHEME_ALL == 0, singleProxy_must_be_first_option);

// Converts a proxy server description |dict| as passed by the API caller
// (e.g. for the http proxy in the rules element) and converts it to a
// ProxyServer. Returns true if successful.
bool GetProxyServer(const DictionaryValue* dict,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* proxy_server) {
  std::string scheme_string;  // optional.
  dict->GetString("scheme", &scheme_string);

  net::ProxyServer::Scheme scheme =
      net::ProxyServer::GetSchemeFromURI(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  std::string host;
  if (!dict->GetString("host", &host))
    return false;

  int port;  // optional.
  if (!dict->GetInteger("port", &port))
    port = net::ProxyServer::GetDefaultPortForScheme(scheme);

  *proxy_server = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

// Converts a proxy "rules" element passed by the API caller into a proxy
// configuration string that can be used by the proxy subsystem (see
// proxy_config.h). Returns true if successful.
bool GetProxyRules(DictionaryValue* proxy_rules, std::string* out) {
  if (!proxy_rules)
    return false;

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_dict holds the
  // DictionaryValues which in turn contain proxy server descriptions, and
  // proxy_server holds ProxyServer structs containing those descriptions.
  bool has_proxy[SCHEME_MAX + 1];
  DictionaryValue* proxy_dict[SCHEME_MAX + 1];
  net::ProxyServer proxy_server[SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= SCHEME_MAX; ++i) {
    has_proxy[i] = proxy_rules->GetDictionary(field_name[i], &proxy_dict[i]);
    if (has_proxy[i]) {
      net::ProxyServer::Scheme default_scheme =
          (i != SCHEME_SOCKS) ? net::ProxyServer::SCHEME_HTTP
                              : net::ProxyServer::SCHEME_SOCKS5;
      if (!GetProxyServer(proxy_dict[i], default_scheme, &proxy_server[i]))
        return false;
    }
  }

  // Handle case that only singleProxy is specified.
  if (has_proxy[SCHEME_ALL]) {
    for (size_t i = 1; i <= SCHEME_MAX; ++i) {
      if (has_proxy[i]) {
        LOG(ERROR) << "Proxy rule for " << field_name[SCHEME_ALL] << " and "
                   << field_name[i] << " cannot be set at the same time.";
        return false;
      }
    }
    *out = proxy_server[SCHEME_ALL].ToURI();
    return true;
  }

  // Handle case that anything but singleProxy is specified.

  // Build the proxy preference string.
  std::string proxy_pref;
  for (size_t i = 1; i <= SCHEME_MAX; ++i) {
    if (has_proxy[i]) {
      // http=foopy:4010;ftp=socks://foopy2:80
      if (!proxy_pref.empty())
        proxy_pref.append(";");
      proxy_pref.append(scheme_name[i]);
      proxy_pref.append("=");
      proxy_pref.append(proxy_server[i].ToURI());
    }
  }

  *out = proxy_pref;
  return true;
}

}  // namespace

void ProxySettingsFunction::ApplyPreference(const char* pref_path,
                                            Value* pref_value,
                                            bool incognito) {
  profile()->GetExtensionService()->extension_prefs()->
      SetExtensionControlledPref(extension_id(), pref_path, incognito,
                                 pref_value);
}

void ProxySettingsFunction::RemovePreference(const char* pref_path,
                                             bool incognito) {
  profile()->GetExtensionService()->extension_prefs()->
      RemoveExtensionControlledPref(extension_id(), pref_path, incognito);
}

bool UseCustomProxySettingsFunction::RunImpl() {
  DictionaryValue* proxy_config;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &proxy_config));

  bool incognito = false;  // Optional argument, defaults to false.
  if (HasOptionalArgument(1)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &incognito));
  }

  std::string proxy_mode;
  proxy_config->GetString("mode", &proxy_mode);
  ProxyPrefs::ProxyMode mode_enum;
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, &mode_enum)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode << ". "
               << "Setting custom proxy settings failed.";
    return false;
  }

  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary("pacScript", &pac_dict);
  std::string pac_url;
  if (pac_dict && !pac_dict->GetString("url", &pac_url)) {
    LOG(ERROR) << "'pacScript' requires a 'url' field. "
               << "Setting custom proxy settings failed.";
    return false;
  }

  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary("rules", &proxy_rules);
  std::string proxy_rules_string;
  if (proxy_rules && !GetProxyRules(proxy_rules, &proxy_rules_string)) {
    LOG(ERROR) << "Invalid 'rules' specified. "
               << "Setting custom proxy settings failed.";
    return false;
  }

  // not supported, yet.
  std::string bypass_list;

  DictionaryValue* result_proxy_config = NULL;
  switch (mode_enum) {
    case ProxyPrefs::MODE_DIRECT:
      result_proxy_config = ProxyConfigDictionary::CreateDirect();
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      result_proxy_config = ProxyConfigDictionary::CreateAutoDetect();
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      if (!pac_dict) {
        LOG(ERROR) << "Proxy mode 'pac_script' requires a 'pacScript' field. "
                   << "Setting custom proxy settings failed.";
        return false;
      }
      result_proxy_config = ProxyConfigDictionary::CreatePacScript(pac_url);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (!proxy_rules) {
        LOG(ERROR) << "Proxy mode 'fixed_servers' requires a 'rules' field. "
                   << "Setting custom proxy settings failed.";
        return false;
      }
      result_proxy_config = ProxyConfigDictionary::CreateFixedServers(
          proxy_rules_string, bypass_list);
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      result_proxy_config = ProxyConfigDictionary::CreateSystem();
      break;
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
  if (!result_proxy_config)
    return false;

  ApplyPreference(prefs::kProxy, result_proxy_config, incognito);
  return true;
}

bool RemoveCustomProxySettingsFunction::RunImpl() {
  bool incognito = false;
  if (HasOptionalArgument(0)) {
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &incognito));
  }

  RemovePreference(prefs::kProxy, incognito);
  return true;
}
