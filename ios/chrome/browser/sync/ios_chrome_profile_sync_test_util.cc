// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"

#include "base/bind.h"
#include "components/browser_sync/browser/profile_sync_service_mock.h"
#include "components/browser_sync/browser/profile_sync_test_util.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/signin_manager_wrapper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"

ProfileSyncService::InitParams CreateProfileSyncServiceParamsForTest(
    scoped_ptr<sync_driver::SyncClient> sync_client,
    ios::ChromeBrowserState* browser_state) {
  ProfileSyncService::InitParams init_params;

  init_params.signin_wrapper = make_scoped_ptr(new SigninManagerWrapper(
      ios::SigninManagerFactory::GetForBrowserState(browser_state)));
  init_params.oauth2_token_service =
      OAuth2TokenServiceFactory::GetForBrowserState(browser_state);
  init_params.start_behavior = browser_sync::MANUAL_START;
  init_params.sync_client =
      sync_client ? std::move(sync_client)
                  : make_scoped_ptr(new IOSChromeSyncClient(browser_state));
  init_params.network_time_update_callback =
      base::Bind(&browser_sync::EmptyNetworkTimeUpdate);
  init_params.base_directory = browser_state->GetStatePath();
  init_params.url_request_context = browser_state->GetRequestContext();
  init_params.debug_identifier = browser_state->GetDebugName();
  init_params.channel = ::GetChannel();
  init_params.db_thread =
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB);
  init_params.file_thread =
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE);
  init_params.blocking_pool = web::WebThread::GetBlockingPool();

  return init_params;
}

scoped_ptr<KeyedService> BuildMockProfileSyncService(
    web::BrowserState* context) {
  return make_scoped_ptr(
      new ProfileSyncServiceMock(CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context))));
}
