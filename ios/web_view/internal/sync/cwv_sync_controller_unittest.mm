// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/device_id_helper.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"
#include "components/sync/driver/fake_sync_client.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVSyncControllerTest : public PlatformTest {
 protected:
  CWVSyncControllerTest()
      : browser_state_(/*off_the_record=*/false),
        signin_client_(browser_state_.GetPrefs()),
        token_service_(browser_state_.GetPrefs()),
        gaia_cookie_manager_service_(&token_service_,
                                     "cookie-source",
                                     &signin_client_),
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_service_,
                        &gaia_cookie_manager_service_) {
    web_state_.SetBrowserState(&browser_state_);

    browser_sync::ProfileSyncService::InitParams init_params;
    init_params.start_behavior = browser_sync::ProfileSyncService::MANUAL_START;
    init_params.sync_client = std::make_unique<syncer::FakeSyncClient>();
    init_params.url_loader_factory = browser_state_.GetSharedURLLoaderFactory();
    init_params.network_time_update_callback = base::DoNothing();
    init_params.signin_scoped_device_id_callback = base::BindRepeating(
        &signin::GetSigninScopedDeviceId, browser_state_.GetPrefs());
    profile_sync_service_ =
        std::make_unique<browser_sync::ProfileSyncServiceMock>(&init_params);

    account_tracker_service_.Initialize(browser_state_.GetPrefs(),
                                        base::FilePath());

    sync_controller_ = [[CWVSyncController alloc]
        initWithProfileSyncService:profile_sync_service_.get()
             accountTrackerService:&account_tracker_service_
                     signinManager:&signin_manager_
                      tokenService:&token_service_];
  };

  web::TestWebThreadBundle web_thread_bundle_;
  ios_web_view::WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  std::unique_ptr<browser_sync::ProfileSyncServiceMock> profile_sync_service_;
  AccountTrackerService account_tracker_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;
  FakeSigninManager signin_manager_;
  CWVSyncController* sync_controller_;
};

// Verifies CWVSyncControllerDataSource methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DataSourceCallbacks) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id data_source = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));

    [[data_source expect]
                 syncController:sync_controller_
        getAccessTokenForScopes:[OCMArg checkWithBlock:^BOOL(NSArray* scopes) {
          return [scopes containsObject:@"scope1.chromium.org"] &&
                 [scopes containsObject:@"scope2.chromium.org"];
        }]
              completionHandler:[OCMArg any]];

    CWVIdentity* identity =
        [[CWVIdentity alloc] initWithEmail:@"johndoe@chromium.org"
                                  fullName:@"John Doe"
                                    gaiaID:@"1337"];
    [sync_controller_ startSyncWithIdentity:identity dataSource:data_source];

    std::set<std::string> scopes = {"scope1.chromium.org",
                                    "scope2.chromium.org"};
    ProfileOAuth2TokenServiceIOSProvider::AccessTokenCallback callback;
    [sync_controller_ fetchAccessTokenForScopes:scopes callback:callback];

    [data_source verify];
  }
}

}  // namespace ios_web_view
