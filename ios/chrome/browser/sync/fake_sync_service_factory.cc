// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/fake_sync_service_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/driver/fake_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace {

class KeyedFakeSyncService : public KeyedService {
 public:
  KeyedFakeSyncService() {}

  syncer::FakeSyncService* fake_sync_service() { return &fake_sync_service_; }

 private:
  syncer::FakeSyncService fake_sync_service_;
};

}  // namespace

namespace ios {

// static
FakeSyncServiceFactory* FakeSyncServiceFactory::GetInstance() {
  return base::Singleton<FakeSyncServiceFactory>::get();
}

// static
syncer::FakeSyncService* FakeSyncServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<KeyedFakeSyncService*>(
             FakeSyncServiceFactory::GetInstance()->GetServiceForBrowserState(
                 browser_state, true))
      ->fake_sync_service();
}

// static
syncer::FakeSyncService* FakeSyncServiceFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<KeyedFakeSyncService*>(
             FakeSyncServiceFactory::GetInstance()->GetServiceForBrowserState(
                 browser_state, false))
      ->fake_sync_service();
}

FakeSyncServiceFactory::FakeSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "FakeSyncService",
          BrowserStateDependencyManager::GetInstance()) {}

FakeSyncServiceFactory::~FakeSyncServiceFactory() {}

std::unique_ptr<KeyedService> FakeSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return base::WrapUnique(new KeyedFakeSyncService);
}

}  // namespace ios
