// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_FAKE_SYNC_SERVICE_FACTORY_H_
#define IOS_PUBLIC_TEST_FAKE_SYNC_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace sync_driver {
class FakeSyncService;
}

namespace ios {

class FakeSyncServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the singleton FakeSyncServiceFactory instance.
  static FakeSyncServiceFactory* GetInstance();

  // Returns the FakeSyncService associated to |browser_state|.
  static sync_driver::FakeSyncService* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend struct DefaultSingletonTraits<FakeSyncServiceFactory>;

  // BrowserStateKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  FakeSyncServiceFactory();
  ~FakeSyncServiceFactory() override;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncServiceFactory);
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_FAKE_SYNC_SERVICE_FACTORY_H_
