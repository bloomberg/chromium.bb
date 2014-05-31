// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_ANDROID_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_ANDROID_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

using data_reduction_proxy::DataReductionProxyParams;

class DataReductionProxySettingsAndroid;

// BrowserContextKeyedServiceFactory for generating/retrieving
// DataReductionProxyService instances.
class DataReductionProxySettingsFactoryAndroid
    : public BrowserContextKeyedServiceFactory {
 public:
  static DataReductionProxySettingsAndroid* GetForBrowserContext(
      content::BrowserContext* context);

  static bool HasDataReductionProxySettingsAndroid(
      content::BrowserContext* context);

  static DataReductionProxySettingsFactoryAndroid* GetInstance();

 private:
  friend struct DefaultSingletonTraits<
      DataReductionProxySettingsFactoryAndroid>;

  DataReductionProxySettingsFactoryAndroid();

  virtual ~DataReductionProxySettingsFactoryAndroid();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_ANDROID_H_
