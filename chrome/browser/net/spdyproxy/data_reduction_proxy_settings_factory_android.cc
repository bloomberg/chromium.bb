// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_factory_android.h"
#include "base/memory/singleton.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

// static
DataReductionProxySettingsAndroid*
DataReductionProxySettingsFactoryAndroid::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DataReductionProxySettingsAndroid*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
bool DataReductionProxySettingsFactoryAndroid::
HasDataReductionProxySettingsAndroid(
    content::BrowserContext* context) {
  return GetInstance()->GetServiceForBrowserContext(context, false) != NULL;
}

// static
DataReductionProxySettingsFactoryAndroid*
DataReductionProxySettingsFactoryAndroid::GetInstance() {
  return Singleton<DataReductionProxySettingsFactoryAndroid>::get();
}


DataReductionProxySettingsFactoryAndroid::
DataReductionProxySettingsFactoryAndroid()
    : BrowserContextKeyedServiceFactory(
        "DataReductionProxySettingsAndroid",
        BrowserContextDependencyManager::GetInstance()) {
}

DataReductionProxySettingsFactoryAndroid::
~DataReductionProxySettingsFactoryAndroid() {
}

KeyedService* DataReductionProxySettingsFactoryAndroid::BuildServiceInstanceFor(
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

  DataReductionProxySettingsAndroid* settings =
      new DataReductionProxySettingsAndroid(
          new DataReductionProxyParams(flags));
  settings->InitDataReductionProxySettings(profile);
  return settings;
}

