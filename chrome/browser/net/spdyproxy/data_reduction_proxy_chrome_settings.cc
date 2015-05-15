// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Assume that any proxy host ending with this suffix is a Data Reduction Proxy.
const char kDataReductionProxyDefaultHostSuffix[] = ".googlezip.net";

// Searches |proxy_list| for any Data Reduction Proxies, even if they don't
// match a currently configured Data Reduction Proxy.
bool ContainsDataReductionProxyDefaultHostSuffix(
    const net::ProxyList& proxy_list) {
  for (const net::ProxyServer& proxy : proxy_list.GetAll()) {
    if (proxy.is_valid() && !proxy.is_direct() &&
        EndsWith(proxy.host_port_pair().host(),
                 kDataReductionProxyDefaultHostSuffix, true)) {
      return true;
    }
  }
  return false;
}

// Searches |proxy_rules| for any Data Reduction Proxies, even if they don't
// match a currently configured Data Reduction Proxy.
bool ContainsDataReductionProxyDefaultHostSuffix(
    const net::ProxyConfig::ProxyRules& proxy_rules) {
  return ContainsDataReductionProxyDefaultHostSuffix(
             proxy_rules.proxies_for_http) ||
         ContainsDataReductionProxyDefaultHostSuffix(
             proxy_rules.proxies_for_https);
}

}  // namespace

// The Data Reduction Proxy has been turned into a "best effort" proxy,
// meaning it is used only if the effective proxy configuration resolves to
// DIRECT for a URL. It no longer can be a ProxyConfig in the proxy preference
// hierarchy. This method removes the Data Reduction Proxy configuration from
// prefs, if present. |proxy_pref_name| is the name of the proxy pref.
void DataReductionProxyChromeSettings::MigrateDataReductionProxyOffProxyPrefs(
    PrefService* prefs) {
  base::DictionaryValue* dict =
      (base::DictionaryValue*) prefs->GetUserPrefValue(prefs::kProxy);
  if (!dict)
    return;

  // Clear empty "proxy" dictionary created by a bug. See http://crbug/448172.
  if (dict->empty()) {
    prefs->ClearPref(prefs::kProxy);
    return;
  }

  std::string mode;
  if (!dict->GetString("mode", &mode))
    return;
  // Clear "system" proxy entry since this is the default. This entry was
  // created by bug (http://crbug/448172).
  if (ProxyModeToString(ProxyPrefs::MODE_SYSTEM) == mode) {
    prefs->ClearPref(prefs::kProxy);
    return;
  }
  if (ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS) != mode)
    return;
  std::string proxy_server;
  if (!dict->GetString("server", &proxy_server))
    return;
  net::ProxyConfig::ProxyRules proxy_rules;
  proxy_rules.ParseFromString(proxy_server);
  // Clear the proxy pref if it matches a currently configured Data Reduction
  // Proxy, or if the proxy host ends with ".googlezip.net", in order to ensure
  // that any DRP in the pref is cleared even if the DRP configuration was
  // changed. See http://crbug.com/476610.
  if (!Config()->ContainsDataReductionProxy(proxy_rules) &&
      !ContainsDataReductionProxyDefaultHostSuffix(proxy_rules)) {
    return;
  }
  prefs->ClearPref(prefs::kProxy);
}

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings()
    : data_reduction_proxy::DataReductionProxySettings() {
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {
}

void DataReductionProxyChromeSettings::Shutdown() {
  data_reduction_proxy_service()->Shutdown();
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    data_reduction_proxy::DataReductionProxyIOData* io_data,
    PrefService* profile_prefs,
    net::URLRequestContextGetter* request_context_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // On mobile we write Data Reduction Proxy prefs directly to the pref service.
  // On desktop we store Data Reduction Proxy prefs in memory, writing to disk
  // every 60 minutes and on termination. Shutdown hooks must be added for
  // Android and iOS in order for non-zero delays to be supported.
  // (http://crbug.com/408264)
  base::TimeDelta commit_delay = base::TimeDelta();
#else
  base::TimeDelta commit_delay = base::TimeDelta::FromMinutes(60);
#endif

  scoped_ptr<data_reduction_proxy::DataReductionProxyCompressionStats>
      compression_stats = make_scoped_ptr(
          new data_reduction_proxy::DataReductionProxyCompressionStats(
              profile_prefs, ui_task_runner, commit_delay));
  scoped_ptr<data_reduction_proxy::DataReductionProxyService> service =
      make_scoped_ptr(new data_reduction_proxy::DataReductionProxyService(
          compression_stats.Pass(), this, profile_prefs, request_context_getter,
          io_data->io_task_runner()));
  data_reduction_proxy::DataReductionProxySettings::
      InitDataReductionProxySettings(profile_prefs, io_data, service.Pass());
  io_data->SetDataReductionProxyService(
      data_reduction_proxy_service()->GetWeakPtr());

  data_reduction_proxy::DataReductionProxySettings::
      SetCallbackToRegisterSyntheticFieldTrial(
          base::Bind(
              &ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial));
  SetDataReductionProxyAlternativeEnabled(
      data_reduction_proxy::DataReductionProxyParams::
          IsIncludedInAlternativeFieldTrial());
  // TODO(bengr): Remove after M46. See http://crbug.com/445599.
  MigrateDataReductionProxyOffProxyPrefs(profile_prefs);
}

// static
data_reduction_proxy::Client DataReductionProxyChromeSettings::GetClient() {
#if defined(OS_ANDROID)
  return data_reduction_proxy::Client::CHROME_ANDROID;
#elif defined(OS_IOS)
  return data_reduction_proxy::Client::CHROME_IOS;
#elif defined(OS_MACOSX)
  return data_reduction_proxy::Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
  return data_reduction_proxy::Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
  return data_reduction_proxy::Client::CHROME_LINUX;
#elif defined(OS_WIN)
  return data_reduction_proxy::Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
  return data_reduction_proxy::Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
  return data_reduction_proxy::Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
  return data_reduction_proxy::Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
  return data_reduction_proxy::Client::CHROME_QNX;
#else
  return data_reduction_proxy::Client::UNKNOWN;
#endif
}
