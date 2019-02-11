// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

browser_sync::ProfileSyncService::InitParams
CreateProfileSyncServiceParamsForTest(
    ios::ChromeBrowserState* browser_state) {
  browser_sync::ProfileSyncService::InitParams init_params;

  init_params.identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  init_params.start_behavior = browser_sync::ProfileSyncService::MANUAL_START;
  init_params.sync_client =
      std::make_unique<IOSChromeSyncClient>(browser_state);
  init_params.network_time_update_callback = base::DoNothing();
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.debug_identifier = browser_state->GetDebugName();

  return init_params;
}

std::unique_ptr<KeyedService> BuildMockProfileSyncService(
    web::BrowserState* context) {
  return std::make_unique<browser_sync::ProfileSyncServiceMock>(
      CreateProfileSyncServiceParamsForTest(
          ios::ChromeBrowserState::FromBrowserState(context)));
}
