// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/test_keyed_service_provider.h"

#include "components/sync_driver/fake_sync_service.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/test/fake_sync_service_factory.h"

namespace ios {

TestKeyedServiceProvider::TestKeyedServiceProvider() {
}

TestKeyedServiceProvider::~TestKeyedServiceProvider() {
}

void TestKeyedServiceProvider::AssertKeyedFactoriesBuilt() {
  FakeSyncServiceFactory::GetInstance();
}

KeyedServiceBaseFactory* TestKeyedServiceProvider::GetSyncServiceFactory() {
  return FakeSyncServiceFactory::GetInstance();
}

sync_driver::SyncService*
TestKeyedServiceProvider::GetSyncServiceForBrowserState(
    ChromeBrowserState* browser_state) {
  return FakeSyncServiceFactory::GetForBrowserState(browser_state);
}

}  // namespace ios
