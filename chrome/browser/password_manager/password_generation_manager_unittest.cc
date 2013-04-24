// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/prefs/pref_service.h"
#include "chrome/browser/password_manager/password_generation_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestPasswordGenerationManager : public PasswordGenerationManager {
 public:
  explicit TestPasswordGenerationManager(content::WebContents* contents)
      : PasswordGenerationManager(contents) {}
  virtual ~TestPasswordGenerationManager() {}

  virtual void SendStateToRenderer(content::RenderViewHost* host,
                                   bool enabled) OVERRIDE {
    sent_states_.push_back(enabled);
  }

  const std::vector<bool>& GetSentStates() {
    return sent_states_;
  }

  void ClearSentStates() {
    sent_states_.clear();
  }

 private:
  std::vector<bool> sent_states_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordGenerationManager);
};

class PasswordGenerationManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordGenerationManagerTest()
      : ChromeRenderViewHostTestHarness(),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() OVERRIDE {
    TestingProfile* profile = CreateProfile();
    profile->CreateRequestContext();
    browser_context_.reset(profile);

    ChromeRenderViewHostTestHarness::SetUp();
    io_thread_.StartIOThread();

    password_generation_manager_.reset(
        new TestPasswordGenerationManager(web_contents()));
  }

  virtual void TearDown() OVERRIDE {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual TestingProfile* CreateProfile() {
    return new TestingProfile();
  }

  void UpdateState(bool new_renderer) {
    password_generation_manager_->UpdateState(NULL, new_renderer);
  }

 protected:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<TestPasswordGenerationManager> password_generation_manager_;
};

class IncognitoPasswordGenerationManagerTest :
    public PasswordGenerationManagerTest {
 public:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    // Create an incognito profile.
    TestingProfile::Builder builder;
    scoped_ptr<TestingProfile> profile = builder.Build();
    profile->set_incognito(true);
    return profile.release();
  }
};

TEST_F(PasswordGenerationManagerTest, UpdateState) {
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefService* prefs = profile()->GetPrefs();

  // Always set password sync enabled so we can test the behavior of password
  // generation.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();
  syncer::ModelTypeSet preferred_set;
  preferred_set.Put(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);

  // Enabled state remains false, should not sent.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, false);
  UpdateState(false);
  EXPECT_EQ(0u, password_generation_manager_->GetSentStates().size());

  // Enabled state from false to true, should sent true.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);
  UpdateState(false);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_TRUE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();

  // Enabled states remains true, should not sent.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);
  UpdateState(false);
  EXPECT_EQ(0u, password_generation_manager_->GetSentStates().size());

  // Enabled states from true to false, should sent false.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, false);
  UpdateState(false);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_FALSE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();

  // When a new render_view is created, we send the state even if it's the
  // same.
  UpdateState(true);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_FALSE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();
}

TEST_F(PasswordGenerationManagerTest, UpdatePasswordSyncState) {
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefService* prefs = profile()->GetPrefs();

  // Allow this test to control what should get synced.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  // Always set password generation enabled check box so we can test the
  // behavior of password sync.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);

  // Sync some things, but not passwords. Shouldn't send anything since
  // password generation is disabled by default.
  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();
  syncer::ModelTypeSet preferred_set;
  preferred_set.Put(syncer::EXTENSIONS);
  preferred_set.Put(syncer::PREFERENCES);
  sync_service->ChangePreferredDataTypes(preferred_set);
  syncer::ModelTypeSet new_set = sync_service->GetPreferredDataTypes();
  UpdateState(false);
  EXPECT_EQ(0u, password_generation_manager_->GetSentStates().size());

  // Now sync passwords.
  preferred_set.Put(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);
  UpdateState(false);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_TRUE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();

  // Add some additional synced state. Nothing should be sent.
  preferred_set.Put(syncer::THEMES);
  sync_service->ChangePreferredDataTypes(preferred_set);
  UpdateState(false);
  EXPECT_EQ(0u, password_generation_manager_->GetSentStates().size());

  // Disable syncing. This should disable the feature.
  sync_service->DisableForUser();
  UpdateState(false);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_FALSE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();

  // When a new render_view is created, we send the state even if it's the
  // same.
  UpdateState(true);
  EXPECT_EQ(1u, password_generation_manager_->GetSentStates().size());
  EXPECT_FALSE(password_generation_manager_->GetSentStates()[0]);
  password_generation_manager_->ClearSentStates();
}

TEST_F(IncognitoPasswordGenerationManagerTest,
       UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito, and enable syncing. The
  // feature should still be disabled, and nothing will be sent.
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefService* prefs = profile()->GetPrefs();

  // Allow this test to control what should get synced.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  // Always set password generation enabled check box so we can test the
  // behavior of password sync.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);

  browser_sync::SyncPrefs sync_prefs(profile()->GetPrefs());
  sync_prefs.SetSyncSetupCompleted();
  UpdateState(false);
  EXPECT_EQ(0u, password_generation_manager_->GetSentStates().size());
}
