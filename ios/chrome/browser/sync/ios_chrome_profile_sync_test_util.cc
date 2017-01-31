// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"

browser_sync::ProfileSyncService::InitParams
CreateProfileSyncServiceParamsForTest(
    std::unique_ptr<syncer::SyncClient> sync_client,
    ios::ChromeBrowserState* browser_state) {
  browser_sync::ProfileSyncService::InitParams init_params;

  init_params.signin_wrapper = base::MakeUnique<SigninManagerWrapper>(
      ios::SigninManagerFactory::GetForBrowserState(browser_state));
  init_params.oauth2_token_service =
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state);
  init_params.start_behavior = browser_sync::ProfileSyncService::MANUAL_START;
  init_params.sync_client =
      sync_client ? std::move(sync_client)
                  : base::MakeUnique<IOSChromeSyncClient>(browser_state);
  init_params.network_time_update_callback =
      base::Bind(&browser_sync::EmptyNetworkTimeUpdate);
  init_params.base_directory = browser_state->GetStatePath();
  init_params.url_request_context = browser_state->GetRequestContext();
  init_params.debug_identifier = browser_state->GetDebugName();
  init_params.channel = ::GetChannel();
  base::SequencedWorkerPool* blocking_pool = web::WebThread::GetBlockingPool();
  init_params.blocking_task_runner =
      blocking_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          blocking_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);

  return init_params;
}

std::unique_ptr<KeyedService> BuildMockProfileSyncService(
    web::BrowserState* context) {
  return base::MakeUnique<browser_sync::ProfileSyncServiceMock>(
      CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context)));
}
