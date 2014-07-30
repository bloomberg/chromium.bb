// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxyUsageStats;

// static
DataReductionProxyChromeSettings*
DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataReductionProxyChromeSettings*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
bool
DataReductionProxyChromeSettingsFactory::HasDataReductionProxyChromeSettings(
    content::BrowserContext* context) {
  return GetInstance()->GetServiceForBrowserContext(context, false) != NULL;
}

// static
DataReductionProxyChromeSettingsFactory*
DataReductionProxyChromeSettingsFactory::GetInstance() {
  return Singleton<DataReductionProxyChromeSettingsFactory>::get();
}


DataReductionProxyChromeSettingsFactory::
    DataReductionProxyChromeSettingsFactory()
    : BrowserContextKeyedServiceFactory(
        "DataReductionProxyChromeSettings",
        BrowserContextDependencyManager::GetInstance()) {
}

DataReductionProxyChromeSettingsFactory::
    ~DataReductionProxyChromeSettingsFactory() {
}

KeyedService* DataReductionProxyChromeSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  int flags = DataReductionProxyParams::kFallbackAllowed;
  if (DataReductionProxyParams::IsIncludedInFieldTrial())
    flags |= DataReductionProxyParams::kAllowed;
  if (DataReductionProxyParams::IsIncludedInAlternativeFieldTrial())
    flags |= DataReductionProxyParams::kAlternativeAllowed;
  if (DataReductionProxyParams::IsIncludedInPromoFieldTrial())
    flags |= DataReductionProxyParams::kPromoAllowed;
  if (DataReductionProxyParams::IsIncludedInHoldbackFieldTrial())
    flags |= DataReductionProxyParams::kHoldback;

  DataReductionProxyParams* params = new DataReductionProxyParams(flags);

  // Takes ownership of params.
  DataReductionProxyChromeSettings* settings =
      new DataReductionProxyChromeSettings(params);
  settings->InitDataReductionProxySettings(profile);
  return settings;
}
