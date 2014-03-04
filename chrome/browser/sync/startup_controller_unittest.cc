// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/startup_controller.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/managed_mode/managed_user_signin_manager_wrapper.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_wrapper.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

static const char kTestUser[] = "test@gmail.com";
static const char kTestToken[] = "testToken";

// These are coupled to the implementation of StartupController's
// GetBackendInitializationStateString which is used by about:sync. We use it
// as a convenient way to verify internal state and that the class is
// outputting the correct values for the debug string.
static const char kStateStringStarted[] = "Started";
static const char kStateStringDeferred[] = "Deferred";
static const char kStateStringNotStarted[] = "Not started";

class FakeManagedUserSigninManagerWrapper
    : public ManagedUserSigninManagerWrapper {
 public:
  FakeManagedUserSigninManagerWrapper()
      : ManagedUserSigninManagerWrapper(NULL) {}
  virtual std::string GetEffectiveUsername() const OVERRIDE {
    return account_;
  }

  virtual std::string GetAccountIdToUse() const OVERRIDE {
    return account_;
  }

  void set_account(const std::string& account) { account_ = account; }

 private:
  std::string account_;
};

class StartupControllerTest : public testing::Test {
 public:
  StartupControllerTest() : started_(false) {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    sync_prefs_.reset(new SyncPrefs(profile_->GetPrefs()));
    token_service_.reset(
        static_cast<FakeProfileOAuth2TokenServiceWrapper*>(
            FakeProfileOAuth2TokenServiceWrapper::Build(profile_.get())));
    signin_.reset(new FakeManagedUserSigninManagerWrapper());

    ProfileSyncServiceStartBehavior behavior =
        browser_defaults::kSyncAutoStarts ? AUTO_START : MANUAL_START;
    base::Closure fake_start_backend = base::Bind(
        &StartupControllerTest::FakeStartBackend, base::Unretained(this));
    controller_.reset(new StartupController(behavior, token_service(),
                                            sync_prefs_.get(), signin_.get(),
                                            fake_start_backend));
    controller_->Reset(syncer::UserTypes());
    controller_->OverrideFallbackTimeoutForTest(
        base::TimeDelta::FromSeconds(0));
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    signin_.reset();
    token_service_.reset();
    sync_prefs_.reset();
    started_ = false;
  }

  void FakeStartBackend() {
    started_ = true;
  }

  void ForceDeferredStartup() {
    if (!CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kSyncEnableDeferredStartup)) {
      CommandLine::ForCurrentProcess()->
          AppendSwitch(switches::kSyncEnableDeferredStartup);
      controller_->Reset(syncer::UserTypes());
    }
  }

  bool started() const { return started_; }
  void clear_started() { started_ = false; }
  StartupController* controller() { return controller_.get(); }
  FakeManagedUserSigninManagerWrapper* signin() { return signin_.get(); }
  FakeProfileOAuth2TokenService* token_service() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        token_service_->GetProfileOAuth2TokenService());
  }
  SyncPrefs* sync_prefs() { return sync_prefs_.get(); }
  Profile* profile() { return profile_.get(); }

 private:
  bool started_;
  base::MessageLoop message_loop_;
  scoped_ptr<StartupController> controller_;
  scoped_ptr<FakeManagedUserSigninManagerWrapper> signin_;
  scoped_ptr<FakeProfileOAuth2TokenServiceWrapper> token_service_;
  scoped_ptr<SyncPrefs> sync_prefs_;
  scoped_ptr<TestingProfile> profile_;
};

// Test that sync doesn't start until all conditions are met.
TEST_F(StartupControllerTest, Basic) {
  controller()->TryStart();
  EXPECT_FALSE(started());
  sync_prefs()->SetSyncSetupCompleted();
  controller()->TryStart();
  EXPECT_FALSE(started());
  signin()->set_account(kTestUser);
  controller()->TryStart();
  EXPECT_FALSE(started());
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  const bool deferred_start = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kSyncEnableDeferredStartup);
  controller()->TryStart();
  EXPECT_EQ(!deferred_start, started());
  std::string state(controller()->GetBackendInitializationStateString());
  EXPECT_TRUE(deferred_start ? state == kStateStringDeferred :
                               state == kStateStringStarted);
}

// Test that sync doesn't when suppressed even if all other conditons are met.
TEST_F(StartupControllerTest, Suppressed) {
  sync_prefs()->SetSyncSetupCompleted();
  sync_prefs()->SetStartSuppressed(true);
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringNotStarted,
            controller()->GetBackendInitializationStateString());
}

// Test that sync doesn't when managed even if all other conditons are met.
TEST_F(StartupControllerTest, Managed) {
  sync_prefs()->SetSyncSetupCompleted();
  sync_prefs()->SetManagedForTest(true);
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringNotStarted,
            controller()->GetBackendInitializationStateString());
}

// Test that sync doesn't start until all conditions are met and a
// data type triggers sync startup.
TEST_F(StartupControllerTest, DataTypeTriggered) {
  ForceDeferredStartup();
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringDeferred,
            controller()->GetBackendInitializationStateString());
  controller()->OnDataTypeRequestsSyncStartup(syncer::SESSIONS);
  EXPECT_TRUE(started());
  EXPECT_EQ(kStateStringStarted,
            controller()->GetBackendInitializationStateString());

  // The fallback timer shouldn't result in another invocation of the closure
  // we passed to the StartupController.
  clear_started();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started());
}

// Test that the fallback timer starts sync in the event all
// conditions are met and no data type requests sync.
TEST_F(StartupControllerTest, FallbackTimer) {
  ForceDeferredStartup();
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(started());
}

// Test that we start immediately if sessions is disabled.
TEST_F(StartupControllerTest, NoDeferralWithoutSessionsSync) {
  syncer::ModelTypeSet types(syncer::UserTypes());
  // Disabling sessions means disabling 4 types due to groupings.
  types.Remove(syncer::SESSIONS);
  types.Remove(syncer::PROXY_TABS);
  types.Remove(syncer::TYPED_URLS);
  types.Remove(syncer::MANAGED_USER_SETTINGS);
  sync_prefs()->SetKeepEverythingSynced(false);
  sync_prefs()->SetPreferredDataTypes(syncer::UserTypes(), types);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSyncEnableDeferredStartup);
  controller()->Reset(syncer::UserTypes());
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_TRUE(started());
}

// Sanity check that the fallback timer doesn't fire before startup
// conditions are met.
TEST_F(StartupControllerTest, FallbackTimerWaits) {
  ForceDeferredStartup();
  controller()->TryStart();
  EXPECT_FALSE(started());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started());
}

// Test that sync starts when the user first asks to setup sync (which
// may be implicit due to the platform).
TEST_F(StartupControllerTest, FirstSetup) {
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();

  if (browser_defaults::kSyncAutoStarts) {
    EXPECT_TRUE(started());
  } else {
    controller()->set_setup_in_progress(true);
    controller()->TryStart();
    EXPECT_TRUE(started());
  }
}

// Test that the controller "forgets" that preconditions were met on reset.
TEST_F(StartupControllerTest, Reset) {
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->TryStart();
  controller()->OnDataTypeRequestsSyncStartup(syncer::SESSIONS);
  EXPECT_TRUE(started());
  clear_started();
  controller()->Reset(syncer::UserTypes());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started());
  const bool deferred_start = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kSyncEnableDeferredStartup);
  controller()->TryStart();
  EXPECT_EQ(!deferred_start, started());
  controller()->OnDataTypeRequestsSyncStartup(syncer::SESSIONS);
  EXPECT_TRUE(started());
}

// Test that setup-in-progress tracking is reset properly on Reset.
// This scenario doesn't affect auto-start platforms.
TEST_F(StartupControllerTest, ResetDuringSetup) {
  signin()->set_account(kTestUser);
  token_service()->IssueRefreshTokenForUser(kTestUser, kTestToken);
  controller()->set_setup_in_progress(true);
  controller()->Reset(syncer::UserTypes());
  controller()->TryStart();

  if (!browser_defaults::kSyncAutoStarts) {
    EXPECT_FALSE(started());
    EXPECT_EQ(kStateStringNotStarted,
              controller()->GetBackendInitializationStateString());
  }
}

}  // namespace browser_sync
