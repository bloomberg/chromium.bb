// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "components/sync/driver/sync_service.h"
#include "components/sync/model/fake_model_type_controller_delegate.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/recent_tabs/sessions_sync_user_state.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> CreateSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(chrome_browser_state);
  return std::make_unique<testing::NiceMock<SyncSetupServiceMock>>(
      sync_service);
}

class SessionSyncServiceMockForRecentTabsTableCoordinator
    : public sync_sessions::SessionSyncService {
 public:
  SessionSyncServiceMockForRecentTabsTableCoordinator() {}
  ~SessionSyncServiceMockForRecentTabsTableCoordinator() override {}

  MOCK_CONST_METHOD0(GetGlobalIdMapper, syncer::GlobalIdMapper*());
  MOCK_METHOD0(GetOpenTabsUIDelegate, sync_sessions::OpenTabsUIDelegate*());
  MOCK_METHOD1(SubscribeToForeignSessionsChanged,
               std::unique_ptr<base::CallbackList<void()>::Subscription>(
                   const base::RepeatingClosure& cb));
  MOCK_METHOD0(ScheduleGarbageCollection, void());
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::ModelTypeControllerDelegate>());
  MOCK_METHOD0(GetFaviconCache, sync_sessions::FaviconCache*());
  MOCK_METHOD1(ProxyTabsStateChanged,
               void(syncer::DataTypeController::State state));
  MOCK_METHOD1(SetSyncSessionsGUID, void(const std::string& guid));
};

std::unique_ptr<KeyedService>
BuildMockSessionSyncServiceForRecentTabsTableCoordinator(
    web::BrowserState* context) {
  return std::make_unique<
      testing::NiceMock<SessionSyncServiceMockForRecentTabsTableCoordinator>>();
}

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_CONST_METHOD2(GetSyncedFaviconForPageURL,
                     bool(const std::string& pageurl,
                          scoped_refptr<base::RefCountedMemory>* favicon_png));
  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<const sync_sessions::SyncedSession*>* sessions));
  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));
  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));
  MOCK_METHOD2(GetForeignSession,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionWindow*>* windows));
  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));
  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local));
};

class RecentTabsTableCoordinatorTest : public BlockCleanupTest {
 public:
  RecentTabsTableCoordinatorTest()
      : no_error_(GoogleServiceAuthError::NONE),
        fake_controller_delegate_(syncer::SESSIONS) {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        SyncSetupServiceFactory::GetInstance(),
        base::BindRepeating(&CreateSyncSetupService));
    test_cbs_builder.AddTestingFactory(
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            &BuildMockSessionSyncServiceForRecentTabsTableCoordinator));
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;

    BlockCleanupTest::TearDown();
  }

  void SetupSyncState(BOOL signedIn,
                      BOOL syncEnabled,
                      BOOL syncCompleted,
                      BOOL hasForeignSessions) {
    if (signedIn) {
      identity_test_env_.MakePrimaryAccountAvailable("test@test.com");
    } else if (identity_test_env_.identity_manager()->HasPrimaryAccount()) {
      auto* account_mutator =
          identity_test_env_.identity_manager()->GetPrimaryAccountMutator();

      // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
      DCHECK(account_mutator);
      account_mutator->ClearPrimaryAccount(
          identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
          signin_metrics::SIGNOUT_TEST,
          signin_metrics::SignoutDelete::IGNORE_METRIC);
    }

    SessionSyncServiceMockForRecentTabsTableCoordinator* session_sync_service =
        static_cast<SessionSyncServiceMockForRecentTabsTableCoordinator*>(
            SessionSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));

    // Needed by ProfileSyncService's initialization, triggered during
    // initialization of SyncSetupServiceMock.
    ON_CALL(*session_sync_service, GetControllerDelegate())
        .WillByDefault(Return(fake_controller_delegate_.GetWeakPtr()));

    SyncSetupServiceMock* syncSetupService = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    ON_CALL(*syncSetupService, IsSyncEnabled())
        .WillByDefault(Return(syncEnabled));
    ON_CALL(*syncSetupService, IsDataTypePreferred(syncer::PROXY_TABS))
        .WillByDefault(Return(true));
    ON_CALL(*syncSetupService, GetSyncServiceState())
        .WillByDefault(Return(SyncSetupService::kNoSyncServiceError));

    if (syncCompleted) {
      ON_CALL(*session_sync_service, GetOpenTabsUIDelegate())
          .WillByDefault(Return(&open_tabs_ui_delegate_));
      ON_CALL(open_tabs_ui_delegate_, GetAllForeignSessions(_))
          .WillByDefault(Return(hasForeignSessions));
    }
  }

  void CreateController() {
    coordinator_ = [[RecentTabsCoordinator alloc]
        initWithBaseViewController:nil
                      browserState:chrome_browser_state_.get()];
    [coordinator_ start];
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  GoogleServiceAuthError no_error_;
  IOSChromeScopedTestingLocalState local_state_;
  identity::IdentityTestEnvironment identity_test_env_;

  syncer::FakeModelTypeControllerDelegate fake_controller_delegate_;
  testing::NiceMock<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  // Must be declared *after* |chrome_browser_state_| so it can outlive it.
  RecentTabsCoordinator* coordinator_;
};

TEST_F(RecentTabsTableCoordinatorTest, TestConstructorDestructor) {
  CreateController();
  EXPECT_TRUE(coordinator_);
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedOut) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(NO, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOff) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, NO, NO, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncInProgress) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, NO, NO);
  CreateController();
}
TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithoutSessions) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, YES, NO);
  CreateController();
}

TEST_F(RecentTabsTableCoordinatorTest, TestUserSignedInSyncOnWithSessions) {
  // TODO(crbug.com/907495): Actual test expectations are missing below.
  SetupSyncState(YES, YES, YES, YES);
  CreateController();
}

}  // namespace
