// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/proxy_config_dictionary.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"

namespace {

// Integer to specify the type of proxy settings.
// See ProxyPrefs for possible values and interactions with the other proxy
// preferences.
const char kProxyMode[] = "mode";
// String specifying the proxy server. For a specification of the expected
// syntax see net::ProxyConfig::ProxyRules::ParseFromString().
const char kProxyServer[] = "server";
// URL to the proxy .pac file.
const char kProxyPacUrl[] = "pac_url";
// String containing proxy bypass rules. For a specification of the
// expected syntax see net::ProxyBypassRules::ParseFromString().
const char kProxyBypassList[] = "bypass_list";

}  // namespace

ProxyConfigDictionary::ProxyConfigDictionary(const DictionaryValue* dict)
    : dict_(dict->DeepCopy()) {
}

ProxyConfigDictionary::~ProxyConfigDictionary() {}

bool ProxyConfigDictionary::GetMode(ProxyPrefs::ProxyMode* out) const {
  std::string mode_str;
  return dict_->GetString(kProxyMode, &mode_str)
      && StringToProxyMode(mode_str, out);
}

bool ProxyConfigDictionary::GetPacUrl(std::string* out) const {
  return dict_->GetString(kProxyPacUrl, out);
}

bool ProxyConfigDictionary::GetProxyServer(std::string* out) const {
  return dict_->GetString(kProxyServer, out);
}

bool ProxyConfigDictionary::GetBypassList(std::string* out) const {
  return dict_->GetString(kProxyBypassList, out);
}

// static
DictionaryValue* ProxyConfigDictionary::CreateDirect() {
  return CreateDictionary(ProxyPrefs::MODE_DIRECT, "", "", "");
}

// static
DictionaryValue* ProxyConfigDictionary::CreateAutoDetect() {
  return CreateDictionary(ProxyPrefs::MODE_AUTO_DETECT, "", "", "");
}

// static
DictionaryValue* ProxyConfigDictionary::CreatePacScript(
    const std::string& pac_url) {
  return CreateDictionary(ProxyPrefs::MODE_PAC_SCRIPT, pac_url, "", "");
}

// static
DictionaryValue* ProxyConfigDictionary::CreateFixedServers(
    const std::string& proxy_server,
    const std::string& bypass_list) {
  if (!proxy_server.empty()) {
    return CreateDictionary(
        ProxyPrefs::MODE_FIXED_SERVERS, "", proxy_server, bypass_list);
  } else {
    return CreateDirect();
  }
}

// static
DictionaryValue* ProxyConfigDictionary::CreateSystem() {
  return CreateDictionary(ProxyPrefs::MODE_SYSTEM, "", "", "");
}

// static
DictionaryValue* ProxyConfigDictionary::CreateDictionary(
    ProxyPrefs::ProxyMode mode,
    const std::string& pac_url,
    const std::string& proxy_server,
    const std::string& bypass_list) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kProxyMode, ProxyModeToString(mode));
  if (!pac_url.empty())
    dict->SetString(kProxyPacUrl, pac_url);
  if (!proxy_server.empty())
    dict->SetString(kProxyServer, proxy_server);
  if (!bypass_list.empty())
    dict->SetString(kProxyBypassList, bypass_list);
  return dict;
}
