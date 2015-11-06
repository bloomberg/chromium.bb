// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SERVICES_GCM_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SERVICES_GCM_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace gcm {
class GCMProfileService;
}

namespace ios {
class ChromeBrowserState;
}

// Singleton that owns all GCMProfileService and associates them with
// ios::ChromeBrowserState.
class IOSChromeGCMProfileServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static gcm::GCMProfileService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  static IOSChromeGCMProfileServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<IOSChromeGCMProfileServiceFactory>;

  IOSChromeGCMProfileServiceFactory();
  ~IOSChromeGCMProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeGCMProfileServiceFactory);
};

#endif  // IOS_CHROME_BROWSER_SERVICES_GCM_IOS_CHROME_GCM_PROFILE_SERVICE_FACTORY_H_
