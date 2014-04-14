// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/pref_names.h"

DataReductionProxyChromeConfigurator::DataReductionProxyChromeConfigurator(
    PrefService* prefs) : prefs_(prefs) {
  DCHECK(prefs);
}

DataReductionProxyChromeConfigurator::~DataReductionProxyChromeConfigurator() {
}

void DataReductionProxyChromeConfigurator::Enable(bool restricted,
                                      const std::string& primary_origin,
                                      const std::string& fallback_origin) {
  DCHECK(prefs_);
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  std::string proxy_list;
  std::string trimmed_fallback_origin;
  base::TrimString(fallback_origin, "/", &trimmed_fallback_origin);

  if (restricted) {
    DCHECK(!fallback_origin.empty());
    proxy_list = trimmed_fallback_origin;
  } else {
    std::string trimmed_primary_origin;
    base::TrimString(primary_origin, "/", &trimmed_primary_origin);
    proxy_list = trimmed_primary_origin +
        (fallback_origin.empty() ? "" : "," + trimmed_fallback_origin);
  }

  std::string proxy_server_config = "http=" + proxy_list + ",direct://;";
  dict->SetString("server", proxy_server_config);
  dict->SetString("mode", ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));
  dict->SetString("bypass_list", JoinString(bypass_rules_, ", "));
}

void DataReductionProxyChromeConfigurator::Disable() {
  DCHECK(prefs_);
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  dict->SetString("mode", ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  dict->SetString("server", "");
  dict->SetString("bypass_list", "");
}

void DataReductionProxyChromeConfigurator::AddHostPatternToBypass(
    const std::string& pattern) {
  bypass_rules_.push_back(pattern);
}

void DataReductionProxyChromeConfigurator::AddURLPatternToBypass(
    const std::string& pattern) {
  size_t pos = pattern.find('/');
  if (pattern.find('/', pos + 1) == pos + 1)
    pos = pattern.find('/', pos + 2);

  std::string host_pattern;
  if (pos != std::string::npos)
    host_pattern = pattern.substr(0, pos);
  else
    host_pattern = pattern;

  AddHostPatternToBypass(host_pattern);
}
