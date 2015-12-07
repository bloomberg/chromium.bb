// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_REDUCTION_PROXY_IOS_CHROME_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_REDUCTION_PROXY_IOS_CHROME_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class IOSChromeDataReductionProxySettings;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ios {
class ChromeBrowserState;
}

// Constucts a DataReductionProxySettings object suitable for use with a
// Chrome browser.
class IOSChromeDataReductionProxySettingsFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns a settings object for the given |browser_state|.
  static IOSChromeDataReductionProxySettings* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  // Returns an instance of this factory.
  static IOSChromeDataReductionProxySettingsFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromeDataReductionProxySettingsFactory>;

  IOSChromeDataReductionProxySettingsFactory();
  ~IOSChromeDataReductionProxySettingsFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeDataReductionProxySettingsFactory);
};

#endif  // IOS_CHROME_BROWSER_DATA_REDUCTION_PROXY_IOS_CHROME_DATA_REDUCTION_PROXY_SETTINGS_FACTORY_H_
