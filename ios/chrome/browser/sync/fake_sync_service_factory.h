// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_FAKE_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_FAKE_SYNC_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace sync_driver {
class FakeSyncService;
}

namespace ios {

class ChromeBrowserState;

class FakeSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the singleton FakeSyncServiceFactory instance.
  static FakeSyncServiceFactory* GetInstance();

  // Returns the FakeSyncService associated to |browser_state|.
  static sync_driver::FakeSyncService* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

  // Returns the FakeSyncService associated to |browser_state|, if it exists.
  static sync_driver::FakeSyncService* GetForBrowserStateIfExists(
      ios::ChromeBrowserState* browser_state);

 private:
  friend struct base::DefaultSingletonTraits<FakeSyncServiceFactory>;

  FakeSyncServiceFactory();
  ~FakeSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  scoped_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncServiceFactory);
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_SYNC_FAKE_SYNC_SERVICE_FACTORY_H_
