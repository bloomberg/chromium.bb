// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_proxy_api.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/string_tokenizer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_io_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_errors.h"
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

const char kProxyEventFatal[] = "fatal";
const char kProxyEventError[] = "error";
const char kProxyEventDetails[] = "details";
const char kProxyEventOnProxyError[] = "experimental.proxy.onProxyError";


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

}  // namespace

// static
ExtensionProxyEventRouter* ExtensionProxyEventRouter::GetInstance() {
  return Singleton<ExtensionProxyEventRouter>::get();
}

ExtensionProxyEventRouter::ExtensionProxyEventRouter() {
}

ExtensionProxyEventRouter::~ExtensionProxyEventRouter() {
}

void ExtensionProxyEventRouter::OnProxyError(
    const ExtensionIOEventRouter* event_router,
    int error_code) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(kProxyEventFatal, true);
  dict->SetString(kProxyEventError, net::ErrorToString(error_code));
  dict->SetString(kProxyEventDetails, "");
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  event_router->DispatchEventToRenderers(
      kProxyEventOnProxyError, json_args, GURL());
}

bool UseCustomProxySettingsFunction::GetProxyServer(
    const DictionaryValue* dict,
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
  if (!dict->GetString(kProxyCfgRuleHost, &host16)) {
    LOG(ERROR) << "Could not parse a 'rules.*.host' entry.";
    return false;
  }
  if (!IsStringASCII(host16)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        "Invalid 'rules.???.host' entry '*'. 'host' field supports only ASCII "
        "URLs (encode URLs in Punycode format).",
        UTF16ToUTF8(host16));
    return false;
  }
  std::string host = UTF16ToASCII(host16);

  int port;  // optional.
  if (!dict->GetInteger(kProxyCfgRulePort, &port))
    port = net::ProxyServer::GetDefaultPortForScheme(scheme);

  *proxy_server = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

bool UseCustomProxySettingsFunction::GetProxyRules(DictionaryValue* proxy_rules,
                                                   std::string* out) {
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
      if (!GetProxyServer(proxy_dict[i], default_scheme, &proxy_server[i])) {
        // Don't set |error_| here, as GetProxyServer takes care of that.
        return false;
      }
    }
  }

  // Handle case that only singleProxy is specified.
  if (has_proxy[SCHEME_ALL]) {
    for (size_t i = 1; i <= SCHEME_MAX; ++i) {
      if (has_proxy[i]) {
        error_ = ExtensionErrorUtils::FormatErrorMessage(
            "Proxy rule for * and * cannot be set at the same time.",
            field_name[SCHEME_ALL], field_name[i]);
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


bool UseCustomProxySettingsFunction::JoinUrlList(
    ListValue* list, const std::string& joiner, std::string* out) {
  std::string result;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!result.empty())
      result.append(joiner);
    // TODO(battre): handle UTF-8 (http://crbug.com/72692)
    string16 entry;
    if (!list->GetString(i, &entry)) {
      LOG(ERROR) << "'rules.bypassList' could not be parsed.";
      return false;
    }
    if (!IsStringASCII(entry)) {
      error_ = "'rules.bypassList' supports only ASCII URLs "
               "(encode URLs in Punycode format).";
      return false;
    }
    result.append(UTF16ToASCII(entry));
  }
  *out = result;
  return true;
}

bool UseCustomProxySettingsFunction::GetBypassList(DictionaryValue* proxy_rules,
                                                   std::string* out) {
  if (!proxy_rules)
    return false;

  ListValue* bypass_list;
  if (!proxy_rules->HasKey(kProxyCfgBypassList)) {
    *out = "";
    return true;
  }
  if (!proxy_rules->GetList(kProxyCfgBypassList, &bypass_list)) {
    LOG(ERROR) << "'rules.bypassList' not be parsed.";
    return false;
  }

  return JoinUrlList(bypass_list, ",", out);
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
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode;
    return false;
  }

  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(kProxyCfgPacScript, &pac_dict);

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692)
  string16 pac_url16;
  if (pac_dict &&
      pac_dict->HasKey(kProxyCfgPacScriptUrl) &&
      !pac_dict->GetString(kProxyCfgPacScriptUrl, &pac_url16)) {
    LOG(ERROR) << "'pacScript.url' could not be parsed.";
    return false;
  }
  if (!IsStringASCII(pac_url16)) {
    error_ = "'pacScript.url' supports only ASCII URLs "
             "(encode URLs in Punycode format).";
    return false;
  }
  std::string pac_url = UTF16ToASCII(pac_url16);

  string16 pac_data16;
  if (pac_dict &&
      pac_dict->HasKey(kProxyCfgPacScriptData) &&
      !pac_dict->GetString(kProxyCfgPacScriptData, &pac_data16)) {
    LOG(ERROR) << "'pacScript.data' could not be parsed.";
    return false;
  }
  if (!IsStringASCII(pac_data16)) {
    error_ = "'pacScript.data' supports only ASCII code"
             "(encode URLs in Punycode format).";
    return false;
  }
  std::string pac_data = UTF16ToASCII(pac_data16);

  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(kProxyCfgRules, &proxy_rules);
  std::string proxy_rules_string;
  if (proxy_rules && !GetProxyRules(proxy_rules, &proxy_rules_string)) {
    // Do not set error message as GetProxyRules does that.
    return false;
  }
  std::string bypass_list;
  if (proxy_rules && !GetBypassList(proxy_rules, &bypass_list)) {
    LOG(ERROR) << "Invalid 'bypassList' specified.";
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
        error_ = "Proxy mode 'pac_script' requires a 'pacScript' field.";
        return false;
      }
      std::string url;
      if (!pac_url.empty()) {
        url = pac_url;
      } else if (!pac_data.empty()) {
        if (!CreateDataURLFromPACScript(pac_data, &url)) {
          error_ = "Internal error, at base64 encoding of 'pacScript.data'.";
          return false;
        }
      } else {
        error_ = "Proxy mode 'pac_script' requires a 'pacScript' field with "
                 "either a 'url' field or a 'data' field.";
        return false;
      }
      result_proxy_config = ProxyConfigDictionary::CreatePacScript(url);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (!proxy_rules) {
        error_ = "Proxy mode 'fixed_servers' requires a 'rules' field.";
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

  if (!ConvertToApiFormat(proxy_prefs, out.get())) {
    // Do not set error message as ConvertToApiFormat does that.
    return false;
  }

  result_.reset(out.release());
  return true;
}

bool GetCurrentProxySettingsFunction::ConvertToApiFormat(
    const DictionaryValue* proxy_prefs,
    DictionaryValue* api_proxy_config) {
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
        error_ = "Invalid proxy configuration. Missing PAC URL.";
        return false;
      }
      DictionaryValue* pac_dict = new DictionaryValue;
      if (pac_url.find("data") == 0) {
        std::string pac_data;
        if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
          error_ = "Cannot decode base64-encoded PAC data URL.";
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
        error_ = "Missing proxy servers in configuration.";
        return false;
      }
      if (!ParseRules(proxy_servers, rules_dict.get())) {
        error_ = "Could not parse proxy rules.";
        return false;
      }

      bool hasBypassList = dict.HasBypassList();
      if (hasBypassList) {
        std::string bypass_list_string;
        if (!dict.GetBypassList(&bypass_list_string)) {
          error_ = "Invalid bypassList in configuration.";
          return false;
        }
        ListValue* bypass_list = NULL;
        if (TokenizeToStringList(bypass_list_string, ",;", &bypass_list)) {
          rules_dict->Set(kProxyCfgBypassList, bypass_list);
        } else {
          error_ = "Error parsing bypassList " + bypass_list_string;
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
