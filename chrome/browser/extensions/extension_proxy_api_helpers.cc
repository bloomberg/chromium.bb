// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of helper functions for the Chrome Extensions Proxy Settings
// API.
//
// Throughout this code, we report errors to the user by setting an |error|
// parameter, if and only if these errors can be cause by invalid input
// from the extension and we cannot expect that the extensions API has
// caught this error before. In all other cases we are dealing with internal
// errors and log to LOG(ERROR).

#include "chrome/browser/extensions/extension_proxy_api_helpers.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_proxy_api_constants.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "net/proxy/proxy_config.h"

namespace keys = extension_proxy_api_constants;

namespace extension_proxy_api_helpers {

bool CreateDataURLFromPACScript(const std::string& pac_script,
                                std::string* pac_script_url_base64_encoded) {
  // Encode pac_script in base64.
  std::string pac_script_base64_encoded;
  if (!base::Base64Encode(pac_script, &pac_script_base64_encoded))
    return false;

  // Make it a correct data url.
  *pac_script_url_base64_encoded =
      std::string(keys::kPACDataUrlPrefix) + pac_script_base64_encoded;
  return true;
}

bool CreatePACScriptFromDataURL(
    const std::string& pac_script_url_base64_encoded,
    std::string* pac_script) {
  if (pac_script_url_base64_encoded.find(keys::kPACDataUrlPrefix) != 0)
    return false;

  // Strip constant data-url prefix.
  std::string pac_script_base64_encoded =
      pac_script_url_base64_encoded.substr(strlen(keys::kPACDataUrlPrefix));

  // The rest is a base64 encoded PAC script.
  return base::Base64Decode(pac_script_base64_encoded, pac_script);
}

// Extension Pref -> Browser Pref conversion.

bool GetProxyModeFromExtensionPref(const DictionaryValue* proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error) {
  std::string proxy_mode;

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  proxy_config->GetStringASCII(keys::kProxyConfigMode, &proxy_mode);
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, out)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode;
    return false;
  }
  return true;
}

bool GetPacUrlFromExtensionPref(const DictionaryValue* proxy_config,
                                std::string* out,
                                std::string* error) {
  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(keys::kProxyConfigPacScript, &pac_dict);
  if (!pac_dict)
    return true;

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692).
  string16 pac_url16;
  if (pac_dict->HasKey(keys::kProxyConfigPacScriptUrl) &&
      !pac_dict->GetString(keys::kProxyConfigPacScriptUrl, &pac_url16)) {
    LOG(ERROR) << "'pacScript.url' could not be parsed.";
    return false;
  }
  if (!IsStringASCII(pac_url16)) {
    *error = "'pacScript.url' supports only ASCII URLs "
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = UTF16ToASCII(pac_url16);
  return true;
}

bool GetPacDataFromExtensionPref(const DictionaryValue* proxy_config,
                                 std::string* out,
                                 std::string* error) {
  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(keys::kProxyConfigPacScript, &pac_dict);
  if (!pac_dict)
    return true;

  string16 pac_data16;
  if (pac_dict->HasKey(keys::kProxyConfigPacScriptData) &&
      !pac_dict->GetString(keys::kProxyConfigPacScriptData, &pac_data16)) {
    LOG(ERROR) << "'pacScript.data' could not be parsed.";
    return false;
  }
  if (!IsStringASCII(pac_data16)) {
    *error = "'pacScript.data' supports only ASCII code"
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = UTF16ToASCII(pac_data16);
  return true;
}

bool GetProxyServer(const DictionaryValue* proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error) {
  std::string scheme_string;  // optional.

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  proxy_server->GetStringASCII(keys::kProxyConfigRuleScheme, &scheme_string);

  net::ProxyServer::Scheme scheme =
      net::ProxyServer::GetSchemeFromURI(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  // TODO(battre): handle UTF-8 in hostnames (http://crbug.com/72692).
  string16 host16;
  if (!proxy_server->GetString(keys::kProxyConfigRuleHost, &host16)) {
    LOG(ERROR) << "Could not parse a 'rules.*.host' entry.";
    return false;
  }
  if (!IsStringASCII(host16)) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        "Invalid 'rules.???.host' entry '*'. 'host' field supports only ASCII "
        "URLs (encode URLs in Punycode format).",
        UTF16ToUTF8(host16));
    return false;
  }
  std::string host = UTF16ToASCII(host16);

  int port;  // optional.
  if (!proxy_server->GetInteger(keys::kProxyConfigRulePort, &port))
    port = net::ProxyServer::GetDefaultPortForScheme(scheme);

  *out = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

bool GetProxyRulesStringFromExtensionPref(const DictionaryValue* proxy_config,
                                          std::string* out,
                                          std::string* error) {
  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(keys::kProxyConfigRules, &proxy_rules);
  if (!proxy_rules)
    return true;

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_server holds the
  // respective ProxyServer objects containing those descriptions.
  bool has_proxy[keys::SCHEME_MAX + 1];
  net::ProxyServer proxy_server[keys::SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= keys::SCHEME_MAX; ++i) {
    DictionaryValue* proxy_dict = NULL;
    has_proxy[i] = proxy_rules->GetDictionary(keys::field_name[i],
                                              &proxy_dict);
    if (has_proxy[i]) {
      net::ProxyServer::Scheme default_scheme = net::ProxyServer::SCHEME_HTTP;
      if (!GetProxyServer(proxy_dict, default_scheme,
                          &proxy_server[i], error)) {
        // Don't set |error| here, as GetProxyServer takes care of that.
        return false;
      }
    }
  }

  COMPILE_ASSERT(keys::SCHEME_ALL == 0, singleProxy_must_be_first_option);

  // Handle case that only singleProxy is specified.
  if (has_proxy[keys::SCHEME_ALL]) {
    for (size_t i = 1; i <= keys::SCHEME_MAX; ++i) {
      if (has_proxy[i]) {
        *error = ExtensionErrorUtils::FormatErrorMessage(
            "Proxy rule for * and * cannot be set at the same time.",
            keys::field_name[keys::SCHEME_ALL], keys::field_name[i]);
        return false;
      }
    }
    *out = proxy_server[keys::SCHEME_ALL].ToURI();
    return true;
  }

  // Handle case that anything but singleProxy is specified.

  // Build the proxy preference string.
  std::string proxy_pref;
  for (size_t i = 1; i <= keys::SCHEME_MAX; ++i) {
    if (has_proxy[i]) {
      // http=foopy:4010;ftp=socks5://foopy2:80
      if (!proxy_pref.empty())
        proxy_pref.append(";");
      proxy_pref.append(keys::scheme_name[i]);
      proxy_pref.append("=");
      proxy_pref.append(proxy_server[i].ToURI());
    }
  }

  *out = proxy_pref;
  return true;
}

bool JoinUrlList(ListValue* list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error) {
  std::string result;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!result.empty())
      result.append(joiner);

    // TODO(battre): handle UTF-8 (http://crbug.com/72692).
    string16 entry;
    if (!list->GetString(i, &entry)) {
      LOG(ERROR) << "'rules.bypassList' could not be parsed.";
      return false;
    }
    if (!IsStringASCII(entry)) {
      *error = "'rules.bypassList' supports only ASCII URLs "
               "(encode URLs in Punycode format).";
      return false;
    }
    result.append(UTF16ToASCII(entry));
  }
  *out = result;
  return true;
}

bool GetBypassListFromExtensionPref(const DictionaryValue* proxy_config,
                                    std::string *out,
                                    std::string* error) {
  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(keys::kProxyConfigRules, &proxy_rules);
  if (!proxy_rules)
    return true;

  if (!proxy_rules->HasKey(keys::kProxyConfigBypassList)) {
    *out = "";
    return true;
  }
  ListValue* bypass_list = NULL;
  if (!proxy_rules->GetList(keys::kProxyConfigBypassList, &bypass_list)) {
    LOG(ERROR) << "'rules.bypassList' not be parsed.";
    return false;
  }

  return JoinUrlList(bypass_list, ",", out, error);
}

DictionaryValue* CreateProxyConfigDict(ProxyPrefs::ProxyMode mode_enum,
                                       const std::string& pac_url,
                                       const std::string& pac_data,
                                       const std::string& proxy_rules_string,
                                       const std::string& bypass_list,
                                       std::string* error) {
  DictionaryValue* result_proxy_config = NULL;
  switch (mode_enum) {
    case ProxyPrefs::MODE_DIRECT:
      result_proxy_config = ProxyConfigDictionary::CreateDirect();
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      result_proxy_config = ProxyConfigDictionary::CreateAutoDetect();
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string url;
      if (!pac_url.empty()) {
        url = pac_url;
      } else if (!pac_data.empty()) {
        if (!CreateDataURLFromPACScript(pac_data, &url)) {
          *error = "Internal error, at base64 encoding of 'pacScript.data'.";
          return NULL;
        }
      } else {
        *error = "Proxy mode 'pac_script' requires a 'pacScript' field with "
                 "either a 'url' field or a 'data' field.";
        return NULL;
      }
      result_proxy_config = ProxyConfigDictionary::CreatePacScript(url);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (proxy_rules_string.empty()) {
        *error = "Proxy mode 'fixed_servers' requires a 'rules' field.";
        return NULL;
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
  return result_proxy_config;
}

DictionaryValue* CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_FIXED_SERVERS);

  scoped_ptr<DictionaryValue> extension_proxy_rules(new DictionaryValue);

  std::string proxy_servers;
  if (!proxy_config.GetProxyServer(&proxy_servers)) {
    LOG(ERROR) << "Missing proxy servers in configuration.";
    return NULL;
  }

  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(proxy_servers);

  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      return NULL;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      if (rules.single_proxy.is_valid()) {
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_ALL],
                                   CreateProxyServerDict(rules.single_proxy));
      }
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      if (rules.proxy_for_http.is_valid()) {
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_HTTP],
                                   CreateProxyServerDict(rules.proxy_for_http));
      }
      if (rules.proxy_for_https.is_valid()) {
        extension_proxy_rules->Set(
            keys::field_name[keys::SCHEME_HTTPS],
            CreateProxyServerDict(rules.proxy_for_https));
      }
      if (rules.proxy_for_ftp.is_valid()) {
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_FTP],
                                   CreateProxyServerDict(rules.proxy_for_ftp));
      }
      if (rules.fallback_proxy.is_valid()) {
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_FALLBACK],
                                   CreateProxyServerDict(rules.fallback_proxy));
      }
      break;
  }

  // If we add a new scheme some time, we need to also store a new dictionary
  // representing this scheme in the code above.
  COMPILE_ASSERT(keys::SCHEME_MAX == 4, SCHEME_FORGOTTEN);

  if (proxy_config.HasBypassList()) {
    std::string bypass_list_string;
    if (!proxy_config.GetBypassList(&bypass_list_string)) {
      LOG(ERROR) << "Invalid bypassList in configuration.";
      return NULL;
    }
    ListValue* bypass_list = TokenizeToStringList(bypass_list_string, ",;");
    extension_proxy_rules->Set(keys::kProxyConfigBypassList, bypass_list);
  }

  return extension_proxy_rules.release();
}

DictionaryValue* CreateProxyServerDict(const net::ProxyServer& proxy) {
  scoped_ptr<DictionaryValue> out(new DictionaryValue);
  switch (proxy.scheme()) {
    case net::ProxyServer::SCHEME_HTTP:
      out->SetString(keys::kProxyConfigRuleScheme, "http");
      break;
    case net::ProxyServer::SCHEME_HTTPS:
      out->SetString(keys::kProxyConfigRuleScheme, "https");
      break;
    case net::ProxyServer::SCHEME_SOCKS4:
      out->SetString(keys::kProxyConfigRuleScheme, "socks4");
      break;
    case net::ProxyServer::SCHEME_SOCKS5:
      out->SetString(keys::kProxyConfigRuleScheme, "socks5");
      break;
    case net::ProxyServer::SCHEME_DIRECT:
    case net::ProxyServer::SCHEME_INVALID:
      NOTREACHED();
      return NULL;
  }
  out->SetString(keys::kProxyConfigRuleHost, proxy.host_port_pair().host());
  out->SetInteger(keys::kProxyConfigRulePort, proxy.host_port_pair().port());
  return out.release();
}

DictionaryValue* CreatePacScriptDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_PAC_SCRIPT);

  scoped_ptr<DictionaryValue> pac_script_dict(new DictionaryValue);
  std::string pac_url;
  if (!proxy_config.GetPacUrl(&pac_url)) {
    LOG(ERROR) << "Invalid proxy configuration. Missing PAC URL.";
    return NULL;
  }

  if (pac_url.find("data") == 0) {
    std::string pac_data;
    if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
      LOG(ERROR) << "Cannot decode base64-encoded PAC data URL.";
      return NULL;
    }
    pac_script_dict->SetString(keys::kProxyConfigPacScriptData, pac_data);
  } else {
    pac_script_dict->SetString(keys::kProxyConfigPacScriptUrl, pac_url);
  }
  return pac_script_dict.release();
}

ListValue* TokenizeToStringList(const std::string& in,
                                const std::string& delims) {
  ListValue* out = new ListValue;
  StringTokenizer entries(in, delims);
  while (entries.GetNext())
    out->Append(Value::CreateStringValue(entries.token()));
  return out;
}

}  // namespace extension_proxy_api_helpers
