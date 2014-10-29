// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/url_request/url_request_context_getter.h"

using data_reduction_proxy::Client;
using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

DataReductionProxyChromeSettings::DataReductionProxyChromeSettings(
    DataReductionProxyParams* params) : DataReductionProxySettings(params) {
}

DataReductionProxyChromeSettings::~DataReductionProxyChromeSettings() {
}

void DataReductionProxyChromeSettings::InitDataReductionProxySettings(
    data_reduction_proxy::DataReductionProxyConfigurator* configurator,
    PrefService* profile_prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* request_context) {
  SetProxyConfigurator(configurator);
  DataReductionProxySettings::InitDataReductionProxySettings(
      profile_prefs,
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
