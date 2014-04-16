// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/test/fake_profile.h"
#include "chrome/browser/ui/app_list/test/fake_profile_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingAppListServiceImpl : public AppListServiceImpl {
 public:
  TestingAppListServiceImpl(const CommandLine& command_line,
                            PrefService* local_state,
                            scoped_ptr<ProfileStore> profile_store)
      : AppListServiceImpl(command_line, local_state, profile_store.Pass()),
        showing_for_profile_(NULL) {}

  Profile* showing_for_profile() const {
    return showing_for_profile_;
  }

  void PerformStartupChecks(Profile* profile) {
    AppListServiceImpl::PerformStartupChecks(profile);
  }

  virtual Profile* GetCurrentAppListProfile() OVERRIDE {
    // We don't return showing_for_profile_ here because that is only defined if
    // the app list is visible.
    return NULL;
  }

  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE {
  }

  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE {
    showing_for_profile_ = requested_profile;
    RecordAppListLaunch();
  }

  virtual void DismissAppList() OVERRIDE {
    showing_for_profile_ = NULL;
  }

  virtual bool IsAppListVisible() const OVERRIDE {
    return !!showing_for_profile_;
  }

  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE {
    return NULL;
  }

  virtual AppListControllerDelegate* GetControllerDelegate() OVERRIDE {
    return NULL;
  }

 private:
  Profile* showing_for_profile_;
};

class AppListServiceUnitTest : public testing::Test {
 public:
  AppListServiceUnitTest() {}

  virtual void SetUp() OVERRIDE {
    SetupWithCommandLine(CommandLine(CommandLine::NO_PROGRAM));
  }

 protected:
  void SetupWithCommandLine(const CommandLine& command_line) {
    user_data_dir_ = base::FilePath(FILE_PATH_LITERAL("udd"));
    profile1_.reset(
        new FakeProfile("p1", user_data_dir_.AppendASCII("profile1")));
    profile2_.reset(
        new FakeProfile("p2", user_data_dir_.AppendASCII("profile2")));
    PrefRegistrySimple* pref_registry = new PrefRegistrySimple;

    AppListService::RegisterPrefs(pref_registry);
    profiles::RegisterPrefs(pref_registry);

    base::PrefServiceFactory factory;
    factory.set_user_prefs(make_scoped_refptr(new TestingPrefStore));
    local_state_ = factory.Create(pref_registry).Pass();

    profile_store_ = new FakeProfileStore(user_data_dir_);
    service_.reset(new TestingAppListServiceImpl(
        command_line,
        local_state_.get(),
        scoped_ptr<ProfileStore>(profile_store_)));
  }

  void EnableAppList() {
    service_->EnableAppList(profile1_.get(),
                            AppListService::ENABLE_VIA_COMMAND_LINE);
  }

  base::FilePath user_data_dir_;
  scoped_ptr<PrefService> local_state_;
  FakeProfileStore* profile_store_;
  scoped_ptr<TestingAppListServiceImpl> service_;
  scoped_ptr<FakeProfile> profile1_;
  scoped_ptr<FakeProfile> profile2_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceUnitTest);
};

TEST_F(AppListServiceUnitTest, EnablingStateIsPersisted) {
  EXPECT_FALSE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
  EnableAppList();
  EXPECT_TRUE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
  EXPECT_EQ(profile1_->GetPath(), user_data_dir_.Append(
      local_state_->GetFilePath(prefs::kAppListProfile)));
}

TEST_F(AppListServiceUnitTest, ShowingForProfileLoadsAProfile) {
  profile_store_->LoadProfile(profile1_.get());
  EnableAppList();
  service_->Show();
  EXPECT_EQ(profile1_.get(), service_->showing_for_profile());
  EXPECT_TRUE(service_->IsAppListVisible());
}

TEST_F(AppListServiceUnitTest, RemovedProfileResetsToInitialProfile) {
  EnableAppList();
  profile_store_->RemoveProfile(profile1_.get());
  base::FilePath initial_profile_path =
      user_data_dir_.AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(initial_profile_path,
            service_->GetProfilePath(profile_store_->GetUserDataDir()));
}

TEST_F(AppListServiceUnitTest,
       RemovedProfileResetsToLastUsedProfileIfExists) {
  local_state_->SetString(prefs::kProfileLastUsed, "last-used");
  EnableAppList();
  profile_store_->RemoveProfile(profile1_.get());
  base::FilePath last_used_profile_path =
      user_data_dir_.AppendASCII("last-used");
  EXPECT_EQ(last_used_profile_path,
            service_->GetProfilePath(profile_store_->GetUserDataDir()));
}

TEST_F(AppListServiceUnitTest, SwitchingProfilesPersists) {
  profile_store_->LoadProfile(profile1_.get());
  profile_store_->LoadProfile(profile2_.get());
  EnableAppList();
  service_->SetProfilePath(profile2_->GetPath());
  service_->Show();
  EXPECT_EQ(profile2_.get(), service_->showing_for_profile());
  EXPECT_EQ(profile2_->GetPath(),
            service_->GetProfilePath(profile_store_->GetUserDataDir()));
  service_->SetProfilePath(profile1_->GetPath());
  EXPECT_EQ(profile1_->GetPath(),
            service_->GetProfilePath(profile_store_->GetUserDataDir()));
}

TEST_F(AppListServiceUnitTest, EnableViaCommandLineFlag) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kEnableAppList);
  SetupWithCommandLine(command_line);
  service_->PerformStartupChecks(profile1_.get());
  EXPECT_TRUE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
}

TEST_F(AppListServiceUnitTest, DisableViaCommandLineFlag) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kResetAppListInstallState);
  SetupWithCommandLine(command_line);
  service_->PerformStartupChecks(profile1_.get());
  EXPECT_FALSE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
}

TEST_F(AppListServiceUnitTest, UMAPrefStates) {
  EXPECT_FALSE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
  EXPECT_EQ(AppListService::ENABLE_NOT_RECORDED,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
  EXPECT_EQ(0, local_state_->GetInt64(prefs::kAppListEnableTime));

  service_->EnableAppList(profile1_.get(),
                          AppListService::ENABLE_FOR_APP_INSTALL);

  // After enable, method and time should be recorded.
  EXPECT_TRUE(local_state_->GetBoolean(prefs::kAppLauncherHasBeenEnabled));
  EXPECT_EQ(AppListService::ENABLE_FOR_APP_INSTALL,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
  EXPECT_NE(0, local_state_->GetInt64(prefs::kAppListEnableTime));

  service_->ShowForProfile(profile1_.get());

  // After a regular "show", time should be cleared, so UMA is not re-recorded.
  EXPECT_EQ(AppListService::ENABLE_FOR_APP_INSTALL,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
  EXPECT_EQ(0, local_state_->GetInt64(prefs::kAppListEnableTime));

  // A second enable should be a no-op.
  service_->EnableAppList(profile1_.get(),
                          AppListService::ENABLE_FOR_APP_INSTALL);
  EXPECT_EQ(AppListService::ENABLE_FOR_APP_INSTALL,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
  EXPECT_EQ(0, local_state_->GetInt64(prefs::kAppListEnableTime));

  // An auto-show here should keep the recorded enable method.
  service_->AutoShowForProfile(profile1_.get());
  EXPECT_EQ(AppListService::ENABLE_FOR_APP_INSTALL,
            local_state_->GetInteger(prefs::kAppListEnableMethod));

  // Clear the enable state, so we can enable again.
  local_state_->SetBoolean(prefs::kAppLauncherHasBeenEnabled, false);
  service_->EnableAppList(profile1_.get(),
                          AppListService::ENABLE_FOR_APP_INSTALL);

  EXPECT_EQ(AppListService::ENABLE_FOR_APP_INSTALL,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
  EXPECT_NE(0, local_state_->GetInt64(prefs::kAppListEnableTime));

  // An auto-show here should update the enable method to prevent recording it
  // as ENABLE_FOR_APP_INSTALL.
  service_->AutoShowForProfile(profile1_.get());
  EXPECT_EQ(AppListService::ENABLE_SHOWN_UNDISCOVERED,
            local_state_->GetInteger(prefs::kAppListEnableMethod));
}
