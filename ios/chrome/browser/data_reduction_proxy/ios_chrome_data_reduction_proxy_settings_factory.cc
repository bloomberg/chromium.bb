// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_settings_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/data_reduction_proxy/ios_chrome_data_reduction_proxy_settings.h"

// static
IOSChromeDataReductionProxySettings*
IOSChromeDataReductionProxySettingsFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<IOSChromeDataReductionProxySettings*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeDataReductionProxySettingsFactory*
IOSChromeDataReductionProxySettingsFactory::GetInstance() {
  return base::Singleton<IOSChromeDataReductionProxySettingsFactory>::get();
}

IOSChromeDataReductionProxySettingsFactory::
    IOSChromeDataReductionProxySettingsFactory()
    : BrowserStateKeyedServiceFactory(
          "DataReductionProxySettings",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromeDataReductionProxySettingsFactory::
    ~IOSChromeDataReductionProxySettingsFactory() {}

scoped_ptr<KeyedService>
IOSChromeDataReductionProxySettingsFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return make_scoped_ptr(new IOSChromeDataReductionProxySettings);
}
