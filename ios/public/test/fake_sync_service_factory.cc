// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/fake_sync_service_factory.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync_driver/fake_sync_service.h"

namespace {

class KeyedFakeSyncService : public KeyedService {
 public:
  KeyedFakeSyncService(sync_driver::FakeSyncService* service)
      : fake_sync_service_(service) {
    DCHECK(fake_sync_service_);
  }

  sync_driver::FakeSyncService* fake_sync_service() {
    return fake_sync_service_.get();
  }

 private:
  scoped_ptr<sync_driver::FakeSyncService> fake_sync_service_;
};

}  // namespace

namespace ios {

// static
FakeSyncServiceFactory* FakeSyncServiceFactory::GetInstance() {
  return Singleton<FakeSyncServiceFactory>::get();
}

// static
sync_driver::FakeSyncService* FakeSyncServiceFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<KeyedFakeSyncService*>(
             FakeSyncServiceFactory::GetInstance()->GetServiceForBrowserState(
                 browser_state, true))->fake_sync_service();
}

KeyedService* FakeSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return new KeyedFakeSyncService(new sync_driver::FakeSyncService);
}

FakeSyncServiceFactory::FakeSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "FakeSyncService",
          BrowserStateDependencyManager::GetInstance()) {
}

FakeSyncServiceFactory::~FakeSyncServiceFactory() {
}

}  // namespace ios
