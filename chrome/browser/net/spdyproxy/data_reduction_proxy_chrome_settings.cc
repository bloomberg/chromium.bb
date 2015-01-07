// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/url_request/url_request_context_getter.h"

using data_reduction_proxy::Client;
using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

// The Data Reduction Proxy has been turned into a "best effort" proxy,
// meaning it is used only if the effective proxy configuration resolves to
// DIRECT for a URL. It no longer can be a ProxyConfig in the proxy preference
// hierarchy. This method removes the Data Reduction Proxy configuration from
// prefs, if present. |proxy_pref_name| is the name of the proxy pref.
void MigrateDataReductionProxyOffProxyPrefs(PrefService* prefs) {
  DictionaryPrefUpdate update(prefs, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  std::string mode;
  if (!dict->GetString("mode", &mode))
    return;
  if (ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS) != mode)
    return;
  std::string proxy_server;
  if (!dict->GetString("server", &proxy_server))
    return;
  net::ProxyConfig::ProxyRules proxy_rules;
  proxy_rules.ParseFromString(proxy_server);
  if (!data_reduction_proxy::DataReductionProxyConfigurator::
          ContainsDataReductionProxy(proxy_rules)) {
    return;
  }
  dict->SetString("mode", ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
  dict->SetString("server", "");
  dict->SetString("bypass_list", "");
}

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    DataReductionProxyParams* params) : DataReductionProxySettings(params) {
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    data_reduction_proxy::DataReductionProxyConfigurator* configurator,
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* request_context,
    net::NetLog* net_log,
    data_reduction_proxy::DataReductionProxyEventStore* event_store) {
  SetProxyConfigurator(configurator);
  DataReductionProxySettings::InitDataReductionProxySettings(
      profile_prefs,
      request_context,
      net_log,
      event_store);
  DataReductionProxySettings::SetOnDataReductionEnabledCallback(
      base::Bind(&DataReductionProxyChromeSettings::RegisterSyntheticFieldTrial,
                 base::Unretained(this)));
  SetDataReductionProxyAlternativeEnabled(
      DataReductionProxyParams::IsIncludedInAlternativeFieldTrial());
  // TODO(bengr): Remove after M46. See http://crbug.com/445599.
  MigrateDataReductionProxyOffProxyPrefs(profile_prefs);
}

void DataReductionProxyChromeSettings::RegisterSyntheticFieldTrial(
    bool data_reduction_proxy_enabled) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "DataReductionProxyEnabled",
      data_reduction_proxy_enabled ? "true" : "false");
}

// static
Client DataReductionProxyChromeSettings::GetClient() {
#if defined(OS_ANDROID)
  return Client::CHROME_ANDROID;
#elif defined(OS_IOS)
  return Client::CHROME_IOS;
#elif defined(OS_MACOSX)
  return Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
  return Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
  return Client::CHROME_LINUX;
#elif defined(OS_WIN)
  return Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
  return Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
  return Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
  return Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
  return Client::CHROME_QNX;
#else
  return Client::UNKNOWN;
#endif
}
