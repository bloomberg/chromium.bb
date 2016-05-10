// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_ORIGINS_SEEN_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_ORIGINS_SEEN_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ios {
class ChromeBrowserState;
}

namespace navigation_metrics {
class OriginsSeenService;
}

// Singleton that owns all OriginsSeenService and associates them with
// ios::ChromeBrowserState.
class IOSChromeOriginsSeenServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static navigation_metrics::OriginsSeenService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static IOSChromeOriginsSeenServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromeOriginsSeenServiceFactory>;

  IOSChromeOriginsSeenServiceFactory();
  ~IOSChromeOriginsSeenServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeOriginsSeenServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_ORIGINS_SEEN_SERVICE_FACTORY_H_
