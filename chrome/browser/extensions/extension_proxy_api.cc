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
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_proxy_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config.h"

namespace keys = extension_proxy_api_constants;

namespace {

void TokenizeToStringList(
    const std::string& in, const std::string& delims, ListValue** out) {
  *out = new ListValue;
  StringTokenizer entries(in, delims);
  while (entries.GetNext())
    (*out)->Append(Value::CreateStringValue(entries.token()));
}

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
    const std::string& pac_script_url_base64_encoded, std::string* pac_script) {
  if (pac_script_url_base64_encoded.find(keys::kPACDataUrlPrefix) != 0)
    return false;
  // Strip constant data-url prefix.
  std::string pac_script_base64_encoded =
      pac_script_url_base64_encoded.substr(strlen(keys::kPACDataUrlPrefix));
  // The rest is a base64 encoded PAC script.
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
    ExtensionEventRouterForwarder* event_router,
    ProfileId profile_id,
    int error_code) {
  ListValue args;
  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean(keys::kProxyEventFatal, true);
  dict->SetString(keys::kProxyEventError, net::ErrorToString(error_code));
  dict->SetString(keys::kProxyEventDetails, "");
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  if (profile_id != Profile::kInvalidProfileId) {
    event_router->DispatchEventToRenderers(
        keys::kProxyEventOnProxyError, json_args, profile_id, true, GURL());
  } else {
    event_router->BroadcastEventToRenderers(
        keys::kProxyEventOnProxyError, json_args, GURL());
  }
}

ProxyPrefTransformer::ProxyPrefTransformer() {
}

ProxyPrefTransformer::~ProxyPrefTransformer() {
}


// Extension Pref -> Browser Pref conversion.

bool ProxyPrefTransformer::GetProxyModeFromExtensionPref(
    const DictionaryValue* proxy_config,
    ProxyPrefs::ProxyMode* out,
    std::string* error) const {
  std::string proxy_mode;
  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  proxy_config->GetStringASCII(keys::kProxyCfgMode, &proxy_mode);
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, out)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode;
    return false;
  }
  return true;
}

bool ProxyPrefTransformer::GetPacUrlFromExtensionPref(
    const DictionaryValue* proxy_config,
    std::string* out,
    std::string* error) const {
  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(keys::kProxyCfgPacScript, &pac_dict);
  if (!pac_dict)
    return true;

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692)
  string16 pac_url16;
  if (pac_dict->HasKey(keys::kProxyCfgPacScriptUrl) &&
      !pac_dict->GetString(keys::kProxyCfgPacScriptUrl, &pac_url16)) {
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

bool ProxyPrefTransformer::GetPacDataFromExtensionPref(
    const DictionaryValue* proxy_config,
    std::string* out,
    std::string* error) const {
  DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(keys::kProxyCfgPacScript, &pac_dict);
  if (!pac_dict)
    return true;

  string16 pac_data16;
  if (pac_dict->HasKey(keys::kProxyCfgPacScriptData) &&
      !pac_dict->GetString(keys::kProxyCfgPacScriptData, &pac_data16)) {
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

bool ProxyPrefTransformer::GetProxyServer(
    const DictionaryValue* dict,
    net::ProxyServer::Scheme default_scheme,
    net::ProxyServer* proxy_server,
    std::string* error) const {
  std::string scheme_string;  // optional.
  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in extension_api.json.
  dict->GetStringASCII(keys::kProxyCfgScheme, &scheme_string);

  net::ProxyServer::Scheme scheme =
      net::ProxyServer::GetSchemeFromURI(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  // TODO(battre): handle UTF-8 in hostnames (http://crbug.com/72692)
  string16 host16;
  if (!dict->GetString(keys::kProxyCfgRuleHost, &host16)) {
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
  if (!dict->GetInteger(keys::kProxyCfgRulePort, &port))
    port = net::ProxyServer::GetDefaultPortForScheme(scheme);

  *proxy_server = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

bool ProxyPrefTransformer::GetProxyRulesStringFromExtensionPref(
    const DictionaryValue* proxy_config,
    std::string* out,
    std::string* error) const {
  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(keys::kProxyCfgRules, &proxy_rules);
  if (!proxy_rules)
    return true;

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_dict holds the
  // DictionaryValues which in turn contain proxy server descriptions, and
  // proxy_server holds ProxyServer structs containing those descriptions.
  bool has_proxy[keys::SCHEME_MAX + 1];
  DictionaryValue* proxy_dict[keys::SCHEME_MAX + 1];
  net::ProxyServer proxy_server[keys::SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= keys::SCHEME_MAX; ++i) {
    has_proxy[i] = proxy_rules->GetDictionary(keys::field_name[i],
                                              &proxy_dict[i]);
    if (has_proxy[i]) {
      net::ProxyServer::Scheme default_scheme = net::ProxyServer::SCHEME_HTTP;
      if (!GetProxyServer(proxy_dict[i], default_scheme,
                          &proxy_server[i], error)) {
        // Don't set |error| here, as GetProxyServer takes care of that.
        return false;
      }
    }
  }

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

bool ProxyPrefTransformer::JoinUrlList(ListValue* list,
                                       const std::string& joiner,
                                       std::string* out,
                                       std::string* error) const {
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
      *error = "'rules.bypassList' supports only ASCII URLs "
               "(encode URLs in Punycode format).";
      return false;
    }
    result.append(UTF16ToASCII(entry));
  }
  *out = result;
  return true;
}

bool ProxyPrefTransformer::GetBypassListFromExtensionPref(
    const DictionaryValue* proxy_config,
    std::string *out,
    std::string* error) const {
  DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(keys::kProxyCfgRules, &proxy_rules);
  if (!proxy_rules)
    return true;

  ListValue* bypass_list;
  if (!proxy_rules->HasKey(keys::kProxyCfgBypassList)) {
    *out = "";
    return true;
  }
  if (!proxy_rules->GetList(keys::kProxyCfgBypassList, &bypass_list)) {
    LOG(ERROR) << "'rules.bypassList' not be parsed.";
    return false;
  }

  return JoinUrlList(bypass_list, ",", out, error);
}

Value* ProxyPrefTransformer::ExtensionToBrowserPref(
    ProxyPrefs::ProxyMode mode_enum,
    const std::string& pac_url,
    const std::string& pac_data,
    const std::string& proxy_rules_string,
    const std::string& bypass_list,
    std::string* error) const {
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
  if (!result_proxy_config)
    return NULL;
  return result_proxy_config;
}

Value* ProxyPrefTransformer::ExtensionToBrowserPref(
    const Value* extension_pref,
    std::string* error) {
  CHECK(extension_pref->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* proxy_config =
      static_cast<const DictionaryValue*>(extension_pref);

  ProxyPrefs::ProxyMode mode_enum;
  std::string pac_url;
  std::string pac_data;
  std::string proxy_rules_string;
  std::string bypass_list;
  if (!GetProxyModeFromExtensionPref(proxy_config, &mode_enum, error) ||
      !GetPacUrlFromExtensionPref(proxy_config, &pac_url, error) ||
      !GetPacDataFromExtensionPref(proxy_config, &pac_data, error) ||
      !GetProxyRulesStringFromExtensionPref(proxy_config, &proxy_rules_string,
                                            error) ||
      !GetBypassListFromExtensionPref(proxy_config, &bypass_list, error))
    return NULL;

  return ExtensionToBrowserPref(mode_enum, pac_url, pac_data,
                                proxy_rules_string, bypass_list, error);
}


// Browser Pref -> Extension Pref conversion.

Value* ProxyPrefTransformer::BrowserToExtensionPref(
    const Value* browser_pref) {
  CHECK(browser_pref->IsType(Value::TYPE_DICTIONARY));
  ProxyConfigDictionary dict(static_cast<const DictionaryValue*>(browser_pref));

  ProxyPrefs::ProxyMode mode;
  if (!dict.GetMode(&mode)) {
    LOG(ERROR) << "Cannot determine proxy mode.";
    return NULL;
  }

  scoped_ptr<DictionaryValue> extension_pref(new DictionaryValue);
  extension_pref->SetString(keys::kProxyCfgMode,
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
        LOG(ERROR) << "Invalid proxy configuration. Missing PAC URL.";
        return NULL;
      }
      DictionaryValue* pac_dict = new DictionaryValue;
      if (pac_url.find("data") == 0) {
        std::string pac_data;
        if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
          LOG(ERROR) << "Cannot decode base64-encoded PAC data URL.";
          return NULL;
        }
        pac_dict->SetString(keys::kProxyCfgPacScriptData, pac_data);
      } else {
        pac_dict->SetString(keys::kProxyCfgPacScriptUrl, pac_url);
      }
      extension_pref->Set(keys::kProxyCfgPacScript, pac_dict);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      scoped_ptr<DictionaryValue> rules_dict(new DictionaryValue);

      std::string proxy_servers;
      if (!dict.GetProxyServer(&proxy_servers)) {
        LOG(ERROR) << "Missing proxy servers in configuration.";
        return NULL;
      }
      net::ProxyConfig::ProxyRules config;
      config.ParseFromString(proxy_servers);
      if (!ConvertProxyRules(config, rules_dict.get())) {
        LOG(ERROR) << "Could not parse proxy rules.";
        return NULL;
      }

      bool hasBypassList = dict.HasBypassList();
      if (hasBypassList) {
        std::string bypass_list_string;
        if (!dict.GetBypassList(&bypass_list_string)) {
          LOG(ERROR) << "Invalid bypassList in configuration.";
          return NULL;
        }
        ListValue* bypass_list = NULL;
        TokenizeToStringList(bypass_list_string, ",;", &bypass_list);
        rules_dict->Set(keys::kProxyCfgBypassList, bypass_list);
      }
      extension_pref->Set(keys::kProxyCfgRules, rules_dict.release());
      break;
    }
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
  return extension_pref.release();
}

bool ProxyPrefTransformer::ConvertProxyRules(
    const net::ProxyConfig::ProxyRules& config,
    DictionaryValue* extension_proxy_rules) const {
  DictionaryValue* extension_proxy_server = NULL;
  switch (config.type) {
    case net::ProxyConfig::ProxyRules::TYPE_NO_RULES:
      return false;
    case net::ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      if (config.single_proxy.is_valid()) {
        ConvertProxyServer(config.single_proxy, &extension_proxy_server);
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_ALL],
                                   extension_proxy_server);
      }
      break;
    case net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME:
      if (config.proxy_for_http.is_valid()) {
        ConvertProxyServer(config.proxy_for_http, &extension_proxy_server);
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_HTTP],
                                   extension_proxy_server);
      }
      if (config.proxy_for_https.is_valid()) {
        ConvertProxyServer(config.proxy_for_https, &extension_proxy_server);
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_HTTPS],
                                   extension_proxy_server);
      }
      if (config.proxy_for_ftp.is_valid()) {
        ConvertProxyServer(config.proxy_for_ftp, &extension_proxy_server);
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_FTP],
                                   extension_proxy_server);
      }
      if (config.fallback_proxy.is_valid()) {
        ConvertProxyServer(config.fallback_proxy, &extension_proxy_server);
        extension_proxy_rules->Set(keys::field_name[keys::SCHEME_FALLBACK],
                                   extension_proxy_server);
      }
      break;
  }
  COMPILE_ASSERT(keys::SCHEME_MAX == 4, SCHEME_FORGOTTEN);
  return true;
}

void ProxyPrefTransformer::ConvertProxyServer(
    const net::ProxyServer& proxy,
    DictionaryValue** extension_proxy_server) const {
  scoped_ptr<DictionaryValue> out(new DictionaryValue);
  switch (proxy.scheme()) {
    case net::ProxyServer::SCHEME_HTTP:
      out->SetString(keys::kProxyCfgScheme, "http");
      break;
    case net::ProxyServer::SCHEME_HTTPS:
      out->SetString(keys::kProxyCfgScheme, "https");
      break;
    case net::ProxyServer::SCHEME_SOCKS4:
      out->SetString(keys::kProxyCfgScheme, "socks4");
      break;
    case net::ProxyServer::SCHEME_SOCKS5:
      out->SetString(keys::kProxyCfgScheme, "socks5");
      break;
    case net::ProxyServer::SCHEME_DIRECT:
    case net::ProxyServer::SCHEME_INVALID:
      NOTREACHED();
      return;
  }
  out->SetString(keys::kProxyCfgRuleHost, proxy.host_port_pair().host());
  out->SetInteger(keys::kProxyCfgRulePort, proxy.host_port_pair().port());
  (*extension_proxy_server) = out.release();
}
