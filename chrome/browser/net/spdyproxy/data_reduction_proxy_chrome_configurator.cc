// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/common/pref_names.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_service.h"

DataReductionProxyChromeConfigurator::DataReductionProxyChromeConfigurator(
    PrefService* prefs,
    scoped_refptr<base::SequencedTaskRunner> network_task_runner)
    : prefs_(prefs), network_task_runner_(network_task_runner) {
}

DataReductionProxyChromeConfigurator::~DataReductionProxyChromeConfigurator() {
}

void DataReductionProxyChromeConfigurator::Enable(
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  DCHECK(prefs_);
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  // TODO(bengr): Consider relying on the proxy config for all data reduction
  // proxy configuration.
  base::DictionaryValue* dict = update.Get();

  std::vector<std::string> proxies;
  if (!primary_restricted) {
    std::string trimmed_primary;
    base::TrimString(primary_origin, "/", &trimmed_primary);
    if (!trimmed_primary.empty())
      proxies.push_back(trimmed_primary);
  }
  if (!fallback_restricted) {
    std::string trimmed_fallback;
    base::TrimString(fallback_origin, "/", &trimmed_fallback);
    if (!trimmed_fallback.empty())
      proxies.push_back(trimmed_fallback);
  }
  if (proxies.empty()) {
    std::string mode;
    // If already in a disabled mode, do nothing.
    if (dict->GetString("mode", &mode))
      if (ProxyModeToString(ProxyPrefs::MODE_SYSTEM) == mode)
        return;
    Disable();
    return;
  }

  std::string trimmed_ssl;
  base::TrimString(ssl_origin, "/", &trimmed_ssl);

  std::string server = "http=" + JoinString(proxies, ",") + ",direct://;"
      + (ssl_origin.empty() ? "" : ("https=" + trimmed_ssl + ",direct://;"));

  dict->SetString("server", server);
  dict->SetString("mode", ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));
  dict->SetString("bypass_list", JoinString(bypass_rules_, ", "));

  net::ProxyConfig config;
  config.proxy_rules().ParseFromString(server);
  config.proxy_rules().bypass_rules.ParseFromString(
      JoinString(bypass_rules_, ", "));
  // The ID is set to a bogus value. It cannot be left uninitialized, else the
  // config will return invalid.
  net::ProxyConfig::ID unused_id = 1;
  config.set_id(unused_id);
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &DataReductionProxyChromeConfigurator::UpdateProxyConfigOnIO,
          base::Unretained(this),
          config));
}

void DataReductionProxyChromeConfigurator::Disable() {
  DCHECK(prefs_);
  DictionaryPrefUpdate update(prefs_, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  dict->SetString("mode", ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  dict->SetString("server", "");
  dict->SetString("bypass_list", "");
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &DataReductionProxyChromeConfigurator::UpdateProxyConfigOnIO,
          base::Unretained(this),
          config));
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

void DataReductionProxyChromeConfigurator::UpdateProxyConfigOnIO(
    const net::ProxyConfig& config) {
  config_ = config;
}

const net::ProxyConfig&
DataReductionProxyChromeConfigurator::GetProxyConfigOnIO() const {
  return config_;
}
