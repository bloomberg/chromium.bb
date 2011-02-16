// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_proxy_api.h"

#include "base/base64.h"
#include "base/string_util.h"
#include "base/string_tokenizer.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/pref_names.h"
#include "net/proxy/proxy_config.h"

namespace {

// The scheme for which to use a manually specified proxy, not of the proxy URI
// itself.
enum {
  SCHEME_ALL = 0,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FTP,
  SCHEME_FALLBACK,
  SCHEME_MAX = SCHEME_FALLBACK  // Keep this value up to date.
};

// The names of the JavaScript properties to extract from the proxy_rules.
// These must be kept in sync with the SCHEME_* constants.
const char* field_name[] = { "singleProxy",
                             "proxyForHttp",
                             "proxyForHttps",
                             "proxyForFtp",
                             "fallbackProxy" };

// The names of the schemes to be used to build the preference value string
// for manual proxy settings.  These must be kept in sync with the SCHEME_*
// constants.
const char* scheme_name[] = { "*error*",
                              "http",
                              "https",
                              "ftp",
                              "socks" };

// String literals in dictionaries used to communicate with extension.
const char kProxyCfgMode[] = "mode";
const char kProxyCfgPacScript[] = "pacScript";
const char kProxyCfgPacScriptUrl[] = "url";
const char kProxyCfgPacScriptData[] = "data";
const char kProxyCfgRules[] = "rules";
const char kProxyCfgRuleHost[] = "host";
const char kProxyCfgRulePort[] = "port";
const char kProxyCfgBypassList[] = "bypassList";
const char kProxyCfgScheme[] = "scheme";

const char kPACDataUrlPrefix[] =
    "data:application/x-ns-proxy-autoconfig;base64,";

COMPILE_ASSERT(SCHEME_MAX == SCHEME_FALLBACK,
               SCHEME_MAX_must_equal_SCHEME_FALLBACK);
COMPILE_ASSERT(arraysize(field_name) == SCHEME_MAX + 1,
               field_name_array_is_wrong_size);
COMPILE_ASSERT(arraysize(scheme_name) == SCHEME_MAX + 1,
               scheme_name_array_is_wrong_size);
COMPILE_ASSERT(SCHEME_ALL == 0, singleProxy_must_be_first_option);

bool TokenizeToStringList(
    const std::string& in, const std::string& delims, ListValue** out) {
  scoped_ptr<ListValue> result(new ListValue);
  StringTokenizer entries(in, delims);
  while (entries.GetNext()) {
    result->Append(Value::CreateStringValue(entries.token()));
  }
  *out = result.release();
  return true;
}

bool JoinStringList(
    ListValue* list, const std::string& joiner, std::string* out) {
  std::string result;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!result.empty())
      result.append(joiner);
    // TODO(battre): handle UTF-8 (http://crbug.com/72692)
    string16 entry;
    if (!list->GetString(i, &entry))
      return false;
    if (!IsStringASCII(entry))
      return false;
    result.append(UTF16ToASCII(entry));
  }
  *out = result;
  return true;
}

bool CreateDataURLFromPACScript(const std::string& pac_script,
                                std::string* pac_script_url_base64_encoded) {
  std::string pac_script_base64_encoded;
  if (!base::Base64Encode(pac_script, &pac_script_base64_encoded))
    return false;
  *pac_script_url_base64_encoded =
      std::string(kPACDataUrlPrefix) + pac_script_base64_encoded;
  return true;
}

bool CreatePACScriptFromDataURL(
    const std::string& pac_script_url_base64_encoded, std::string* pac_script) {
  if (pac_script_url_base64_encoded.find(kPACDataUrlPrefix) != 0) {
    return false;
  }
  std::string pac_script_base64_encoded =
      pac_script_url_base64_encoded.substr(strlen(kPACDataUrlPrefix));
  return base::Base64Decode(pac_script_base64_encoded, pac_script);
}

// Converts a proxy server description |dict| as passed by the API caller
// (e.g. for the http proxy in the rules element) and converts it to a
// ProxyServer. Returns true if successful.
bool GetProxyServer(const DictionaryValue* dict,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* proxy_server) {
  std::string scheme_string;  // optional.
  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  dict->GetStringASCII(kProxyCfgScheme, &scheme_string);

  net::ProxyServer::Scheme scheme =
      net::ProxyServer::GetSchemeFromURI(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  // TODO(battre): handle UTF-8 in hostnames (http://crbug.com/72692)
  string16 host16;
  if (!dict->GetString(kProxyCfgRuleHost, &host16))
    return false;
  if (!IsStringASCII(host16))
    return false;
  std::string host = UTF16ToASCII(host16);

  int port;  // optional.
  if (!dict->GetInteger(kProxyCfgRulePort, &port))
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
      net::ProxyServer::Scheme default_scheme = net::ProxyServer::SCHEME_HTTP;
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
      // http=foopy:4010;ftp=socks5://foopy2:80
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

// Creates a string of the "bypassList" entries of a ProxyRules object (see API
// documentation) by joining the elements with commas.
// Returns true if successful (i.e. string could be delivered or no "bypassList"
// exists in the |proxy_rules|).
bool GetBypassList(DictionaryValue* proxy_rules, std::string* out) {
  if (!proxy_rules)
    return false;

  ListValue* bypass_list;
  if (!proxy_rules->HasKey(kProxyCfgBypassList)) {
    *out = "";
    return true;
  }
  if (!proxy_rules->GetList(kProxyCfgBypassList, &bypass_list))
    return false;

  return JoinStringList(bypass_list, ",", out);
}

}  // namespace

void ProxySettingsFunction::ApplyPreference(const char* pref_path,
                                            Value* pref_value,
                                            bool incognito) {
  Profile* use_profile = profile();
  if (use_profile->IsOffTheRecord())
    use_profile = use_profile->GetOriginalProfile();

  use_profile->GetExtensionService()->extension_prefs()->
      SetExtensionControlledPref(extension_id(), pref_path, incognito,
                                 pref_value);
}

void ProxySettingsFunction::RemovePreference(const char* pref_path,
                                             bool incognito) {
  Profile* use_profile = profile();
  if (use_profile->IsOffTheRecord())
    use_profile = use_profile->GetOriginalProfile();

  use_profile->GetExtensionService()->extension_prefs()->
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
  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  proxy_config->GetStringASCII(kProxyCfgMode, &proxy_mode);
  ProxyPrefs::ProxyMode mode_enum;
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, &mode_enum)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode << ". "
               << "Setting custom proxy settings failed.";
    return false;
  }

  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(kProxyCfgPacScript, &pac_dict);

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692)
  string16 pac_url16;
  if (pac_dict &&
      pac_dict->HasKey(kProxyCfgPacScriptUrl) &&
      !pac_dict->GetString(kProxyCfgPacScriptUrl, &pac_url16)) {
    LOG(ERROR) << "'pacScript' requires a 'url' field. "
               << "Setting custom proxy settings failed.";
    return false;
  }
  if (!IsStringASCII(pac_url16)) {
    LOG(ERROR) << "Only ASCII URLs are supported, yet";
    return false;
  }
  std::string pac_url = UTF16ToASCII(pac_url16);

  string16 pac_data16;
  if (pac_dict &&
      pac_dict->HasKey(kProxyCfgPacScriptData) &&
      !pac_dict->GetString(kProxyCfgPacScriptData, &pac_data16)) {
    LOG(ERROR) << "'pacScript' could not parse 'data' field.";
    return false;
  }
  if (!IsStringASCII(pac_data16)) {
    LOG(ERROR) << "Only ASCII pac data are supported, yet";
    return false;
  }
  std::string pac_data = UTF16ToASCII(pac_data16);

  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(kProxyCfgRules, &proxy_rules);
  std::string proxy_rules_string;
  if (proxy_rules && !GetProxyRules(proxy_rules, &proxy_rules_string)) {
    LOG(ERROR) << "Invalid 'rules' specified. "
               << "Setting custom proxy settings failed.";
    return false;
  }
  std::string bypass_list;
  if (proxy_rules && !GetBypassList(proxy_rules, &bypass_list)) {
    LOG(ERROR) << "Invalid 'bypassList' specified. "
               << "Setting custom proxy settings failed.";
    return false;
  }

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
      std::string url;
      if (!pac_url.empty()) {
        url = pac_url;
      } else if (!pac_data.empty()) {
        if (!CreateDataURLFromPACScript(pac_data, &url)) {
          LOG(ERROR) << "Error at base64 encoding pac data.";
          return false;
        }
      } else {
        LOG(ERROR) << "Proxy mode 'pac_script' requires a 'pacScript' field "
                   << "with either a 'url' field or a 'data' field. "
                   << "Setting custom proxy settings failed.";
        return false;
      }
      result_proxy_config = ProxyConfigDictionary::CreatePacScript(url);
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

bool GetCurrentProxySettingsFunction::RunImpl() {
  bool incognito = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &incognito));

  // This is how it is stored in the PrefStores:
  const DictionaryValue* proxy_prefs;

  Profile* use_profile = profile();
  if (use_profile->IsOffTheRecord())
    use_profile = use_profile->GetOriginalProfile();

  PrefService* prefs = incognito ? use_profile->GetOffTheRecordPrefs()
                                 : use_profile->GetPrefs();
  proxy_prefs = prefs->GetDictionary(prefs::kProxy);

  // This is how it is presented to the API caller:
  scoped_ptr<DictionaryValue> out(new DictionaryValue);

  if (!ConvertToApiFormat(proxy_prefs, out.get()))
    return false;

  result_.reset(out.release());
  return true;
}

bool GetCurrentProxySettingsFunction::ConvertToApiFormat(
    const DictionaryValue* proxy_prefs,
    DictionaryValue* api_proxy_config) const {
  ProxyConfigDictionary dict(proxy_prefs);

  ProxyPrefs::ProxyMode mode;
  if (!dict.GetMode(&mode)) {
    LOG(ERROR) << "Cannot determine proxy mode.";
    return false;
  }
  api_proxy_config->SetString(kProxyCfgMode,
                              ProxyPrefs::ProxyModeToString(mode));

  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
    case ProxyPrefs::MODE_AUTO_DETECT:
    case ProxyPrefs::MODE_SYSTEM:
      // These modes have no further parameters.
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url;
      if (!dict.GetPacUrl(&pac_url)) {
        LOG(ERROR) << "Missing pac url";
        return false;
      }
      DictionaryValue* pac_dict = new DictionaryValue;
      if (pac_url.find("data") == 0) {
        std::string pac_data;
        if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
          LOG(ERROR) << "Cannot decode base64-encoded pac data url";
          return false;
        }
        pac_dict->SetString(kProxyCfgPacScriptData, pac_data);
      } else {
        pac_dict->SetString(kProxyCfgPacScriptUrl, pac_url);
      }
      api_proxy_config->Set(kProxyCfgPacScript, pac_dict);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      scoped_ptr<DictionaryValue> rules_dict(new DictionaryValue);

      std::string proxy_servers;
      if (!dict.GetProxyServer(&proxy_servers)) {
        LOG(ERROR) << "Missing proxy servers in configuration";
        return false;
      }
      if (!ParseRules(proxy_servers, rules_dict.get())) {
        LOG(ERROR) << "Could not parse proxy rules";
        return false;
      }

      bool hasBypassList = dict.HasBypassList();
      if (hasBypassList) {
        std::string bypass_list_string;
        if (!dict.GetBypassList(&bypass_list_string)) {
          LOG(ERROR) << "Invalid bypassList in configuration";
          return false;
        }
        ListValue* bypass_list = NULL;
        if (TokenizeToStringList(bypass_list_string, ",;", &bypass_list)) {
          rules_dict->Set(kProxyCfgBypassList, bypass_list);
        } else {
          LOG(ERROR) << "Error parsing bypassList " << bypass_list_string;
          return false;
        }
      }
      api_proxy_config->Set(kProxyCfgRules, rules_dict.release());
      break;
    }
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
  return true;
}

bool GetCurrentProxySettingsFunction::ParseRules(const std::string& rules,
                                                 DictionaryValue* out) const {
  net::ProxyConfig::ProxyRules config;
  config.ParseFromString(rules);
  switch (config.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      return false;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      if (config.single_proxy.is_valid()) {
        out->Set(field_name[SCHEME_ALL],
                 ConvertToDictionary(config.single_proxy));
      }
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      if (config.proxy_for_http.is_valid()) {
        out->Set(field_name[SCHEME_HTTP],
                 ConvertToDictionary(config.proxy_for_http));
      }
      if (config.proxy_for_https.is_valid()) {
        out->Set(field_name[SCHEME_HTTPS],
                 ConvertToDictionary(config.proxy_for_https));
      }
      if (config.proxy_for_ftp.is_valid()) {
        out->Set(field_name[SCHEME_FTP],
                 ConvertToDictionary(config.proxy_for_ftp));
      }
      if (config.fallback_proxy.is_valid()) {
        out->Set(field_name[SCHEME_FALLBACK],
                 ConvertToDictionary(config.fallback_proxy));
      }
      COMPILE_ASSERT(SCHEME_MAX == 4, SCHEME_FORGOTTEN);
      break;
  }
  return true;
}

DictionaryValue* GetCurrentProxySettingsFunction::ConvertToDictionary(
    const net::ProxyServer& proxy) const {
  DictionaryValue* out = new DictionaryValue;
  switch (proxy.scheme()) {
    case net::ProxyServer::SCHEME_HTTP:
      out->SetString(kProxyCfgScheme, "http");
      break;
    case net::ProxyServer::SCHEME_HTTPS:
      out->SetString(kProxyCfgScheme, "https");
      break;
    case net::ProxyServer::SCHEME_SOCKS4:
      out->SetString(kProxyCfgScheme, "socks4");
      break;
    case net::ProxyServer::SCHEME_SOCKS5:
      out->SetString(kProxyCfgScheme, "socks5");
      break;
    case net::ProxyServer::SCHEME_DIRECT:
    case net::ProxyServer::SCHEME_INVALID:
      NOTREACHED();
      return out;
  }
  out->SetString(kProxyCfgRuleHost, proxy.host_port_pair().host());
  out->SetInteger(kProxyCfgRulePort, proxy.host_port_pair().port());
  return out;
}
