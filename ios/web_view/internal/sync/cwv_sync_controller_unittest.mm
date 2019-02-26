// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#include <memory>
#include <set>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/sync/driver/sync_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {
namespace {

using testing::_;
using testing::Invoke;
using testing::Return;

}  // namespace

class CWVSyncControllerTest : public PlatformTest {
 protected:
  CWVSyncControllerTest()
      : browser_state_(
            // Using comma-operator to perform required initialization before
            // creating browser_state.
            (InitializeLocaleAndResources(), /*off_the_record=*/false)),
        signin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT,
            identity_test_env_.identity_manager()) {
    web_state_.SetBrowserState(&browser_state_);

    browser_sync::ProfileSyncService::InitParams init_params;
    init_params.start_behavior = browser_sync::ProfileSyncService::MANUAL_START;
    init_params.sync_client =
        profile_sync_service_bundle_.CreateSyncClientMock();
    init_params.url_loader_factory = browser_state_.GetSharedURLLoaderFactory();
    init_params.network_time_update_callback = base::DoNothing();
    init_params.identity_manager = identity_test_env_.identity_manager();
    profile_sync_service_ =
        std::make_unique<browser_sync::ProfileSyncServiceMock>(
            std::move(init_params));

    EXPECT_CALL(*profile_sync_service_, AddObserver(_))
        .WillOnce(Invoke(this, &CWVSyncControllerTest::AddObserver));

    sync_controller_ = [[CWVSyncController alloc]
          initWithSyncService:profile_sync_service_.get()
              identityManager:identity_test_env_.identity_manager()
        signinErrorController:&signin_error_controller_];
  }

  ~CWVSyncControllerTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
    EXPECT_CALL(*profile_sync_service_, RemoveObserver(_));
  }

  void AddObserver(syncer::SyncServiceObserver* observer) {
    sync_service_observer_ = observer;
  }

  void OnConfigureDone(const syncer::DataTypeManager::ConfigureResult& result) {
    sync_service_observer_->OnSyncConfigurationCompleted(
        profile_sync_service_.get());
  }

  static void InitializeLocaleAndResources() {
    l10n_util::OverrideLocaleWithCocoaLocale();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
        ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  web::TestWebThreadBundle web_thread_bundle_;
  ios_web_view::WebViewBrowserState browser_state_;
  web::TestWebState web_state_;
  browser_sync::ProfileSyncServiceBundle profile_sync_service_bundle_;
  identity::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  std::unique_ptr<browser_sync::ProfileSyncServiceMock> profile_sync_service_;
  CWVSyncController* sync_controller_;
  syncer::SyncServiceObserver* sync_service_observer_;
};

// Verifies CWVSyncControllerDataSource methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DataSourceCallbacks) {
  // [data_source expect] returns an autoreleased object, but it must be
  // destroyed before this test exits to avoid holding on to |sync_controller_|.
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

// Verifies CWVSyncControllerDelegate methods are invoked with the correct
// parameters.
TEST_F(CWVSyncControllerTest, DelegateCallbacks) {
  // [delegate expect] returns an autoreleased object, but it must be destroyed
  // before this test exits to avoid holding on to |sync_controller_|.
  @autoreleasepool {
    id delegate = OCMProtocolMock(@protocol(CWVSyncControllerDelegate));
    sync_controller_.delegate = delegate;

    [[delegate expect] syncControllerDidStartSync:sync_controller_];
    EXPECT_CALL(*profile_sync_service_, OnConfigureDone(_))
        .WillOnce(Invoke(
            this,
            &CWVSyncControllerTest_DelegateCallbacks_Test::OnConfigureDone));
    syncer::DataTypeManager::ConfigureResult result;
    profile_sync_service_->OnConfigureDone(result);
    [[delegate expect]
          syncController:sync_controller_
        didFailWithError:[OCMArg checkWithBlock:^BOOL(NSError* error) {
          return error.code == CWVSyncErrorInvalidGAIACredentials;
        }]];

    // Create authentication error.
    GoogleServiceAuthError auth_error(
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    std::string account_id =
        identity_test_env_.MakePrimaryAccountAvailable("email@example.com")
            .account_id;
    // TODO(crbug.com/930094): Eliminate this.
    identity_test_env_.identity_manager()->LegacyAddAccountFromSystem(
        account_id);
    identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
        account_id, auth_error);

    [[delegate expect] syncController:sync_controller_
                didStopSyncWithReason:CWVStopSyncReasonServer];
    [sync_controller_
        didSignoutWithSourceMetric:signin_metrics::ProfileSignout::
                                       SERVER_FORCED_DISABLE];
    [delegate verify];
  }
}

// Verifies CWVSyncController properly maintains the current syncing user.
TEST_F(CWVSyncControllerTest, CurrentIdentity) {
  CWVIdentity* identity =
      [[CWVIdentity alloc] initWithEmail:@"johndoe@chromium.org"
                                fullName:@"John Doe"
                                  gaiaID:@"1337"];
  id unused_mock = OCMProtocolMock(@protocol(CWVSyncControllerDataSource));
  [sync_controller_ startSyncWithIdentity:identity dataSource:unused_mock];
  CWVIdentity* currentIdentity = sync_controller_.currentIdentity;
  EXPECT_TRUE(currentIdentity);
  EXPECT_NSEQ(identity.email, currentIdentity.email);
  EXPECT_NSEQ(identity.fullName, currentIdentity.fullName);
  EXPECT_NSEQ(identity.gaiaID, currentIdentity.gaiaID);

  [sync_controller_ stopSyncAndClearIdentity];
  EXPECT_FALSE(sync_controller_.currentIdentity);
}

// Verifies CWVSyncController's passphrase API.
TEST_F(CWVSyncControllerTest, Passphrase) {
  EXPECT_CALL(*profile_sync_service_->GetUserSettingsMock(),
              IsPassphraseRequiredForDecryption())
      .WillOnce(Return(true));
  EXPECT_TRUE(sync_controller_.passphraseNeeded);
  EXPECT_CALL(*profile_sync_service_->GetUserSettingsMock(),
              SetDecryptionPassphrase("dummy-passphrase"))
      .WillOnce(Return(true));
  EXPECT_TRUE([sync_controller_ unlockWithPassphrase:@"dummy-passphrase"]);
}

}  // namespace ios_web_view
