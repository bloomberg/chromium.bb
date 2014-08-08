// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "net/url_request/url_request_context_getter.h"

using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    DataReductionProxyParams* params) : DataReductionProxySettings(params) {
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    scoped_ptr<data_reduction_proxy::DataReductionProxyConfigurator>
        configurator,
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* request_context) {
  SetProxyConfigurator(configurator.Pass());
  DataReductionProxySettings::InitDataReductionProxySettings(
      profile_prefs,
      local_state_prefs,
      request_context);
  DataReductionProxySettings::SetOnDataReductionEnabledCallback(
      base::Bind(&DataReductionProxyChromeSettings::RegisterSyntheticFieldTrial,
                 base::Unretained(this)));
  SetDataReductionProxyAlternativeEnabled(
      DataReductionProxyParams::IsIncludedInAlternativeFieldTrial());
}

void DataReductionProxyChromeSettings::RegisterSyntheticFieldTrial(
    bool data_reduction_proxy_enabled) {
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "DataReductionProxyEnabled",
      data_reduction_proxy_enabled ? "true" : "false");
}

// static
std::string DataReductionProxyChromeSettings::GetBuildAndPatchNumber() {
  chrome::VersionInfo version_info;
  std::vector<std::string> version_parts;
  base::SplitString(version_info.Version(), '.', &version_parts);
  if (version_parts.size() != 4)
    return "";
  return version_parts[2] + version_parts[3];
}

// static
std::string DataReductionProxyChromeSettings::GetClient() {
#if defined(OS_ANDROID)
  return data_reduction_proxy::kClientChromeAndroid;
#elif defined(OS_IOS)
  return data_reduction_proxy::kClientChromeIOS;
#else
  return "";
#endif
}
