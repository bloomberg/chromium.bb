// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>  // For CoInitialize().

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/string16.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/shortcut.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class ProfileShortcutManagerTest : public testing::Test {
 protected:
  ProfileShortcutManagerTest()
      : distribution_(NULL),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_shortcut_manager_(NULL),
        profile_info_cache_(NULL),
        fake_user_desktop_(base::DIR_USER_DESKTOP) {
  }

  virtual void SetUp() OVERRIDE {
    CoInitialize(NULL);
    distribution_ = BrowserDistribution::GetDistribution();
    ASSERT_TRUE(distribution_ != NULL);

    TestingBrowserProcess* browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_info_cache_ = profile_manager_->profile_info_cache();
    profile_shortcut_manager_.reset(
        ProfileShortcutManager::Create(profile_manager_->profile_manager()));

    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &exe_path_));
    dest_path_ = profile_info_cache_->GetUserDataDir();
    dest_path_ = dest_path_.Append(FILE_PATH_LITERAL("My profile"));
    file_util::CreateDirectoryW(dest_path_);
    profile_name_ = L"My profile";

    ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           distribution_,
                                           ShellUtil::CURRENT_USER,
                                           &shortcuts_directory_));

    second_dest_path_ = profile_info_cache_->GetUserDataDir();
    second_dest_path_ =
        second_dest_path_.Append(FILE_PATH_LITERAL("My profile 2"));
    file_util::CreateDirectoryW(second_dest_path_);
    second_profile_name_ = L"My profile 2";
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunUntilIdle();

    // Remove all shortcuts except the last (since it will no longer have
    // an appended name).
    const int num_profiles = profile_info_cache_->GetNumberOfProfiles();
    for (int i = 0; i < num_profiles; ++i) {
      const FilePath profile_path =
          profile_info_cache_->GetPathOfProfileAtIndex(0);
      string16 profile_name;
      if (i != num_profiles - 1)
        profile_name = profile_info_cache_->GetNameOfProfileAtIndex(0);
      profile_info_cache_->DeleteProfileFromCache(profile_path);
      RunPendingTasks();
      ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name));
      ASSERT_FALSE(file_util::PathExists(profile_path.Append(
          FILE_PATH_LITERAL("Google Profile.ico"))));
    }
  }

  void RunPendingTasks() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  void SetupDefaultProfileShortcut() {
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name_));
    // A non-badged shortcut for chrome is automatically created with the
    // first profile (for the case when the user deletes their only profile).
    profile_info_cache_->AddProfileToCache(dest_path_, profile_name_,
                                           string16(), 0);
    RunPendingTasks();
    // We now have 1 profile, so we expect a new shortcut with no profile
    // information.
    ValidateProfileShortcut(string16());
  }

  void SetupAndCreateTwoShortcuts() {
    ASSERT_EQ(0, profile_info_cache_->GetNumberOfProfiles());
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name_));
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));

    profile_info_cache_->AddProfileToCache(dest_path_, profile_name_,
                                           string16(), 0);
    profile_info_cache_->AddProfileToCache(second_dest_path_,
                                           second_profile_name_, string16(), 0);
    profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
    RunPendingTasks();
    ValidateProfileShortcut(profile_name_);
    ValidateProfileShortcut(second_profile_name_);
  }

  // Returns the default shortcut path for this profile.
  FilePath GetDefaultShortcutPathForProfile(const string16& profile_name) {
    return shortcuts_directory_.Append(
        profiles::internal::GetShortcutFilenameForProfile(profile_name,
                                                          distribution_));
  }

  // Returns true if the shortcut for this profile exists.
  bool ProfileShortcutExistsAtDefaultPath(const string16& profile_name) {
    return file_util::PathExists(
        GetDefaultShortcutPathForProfile(profile_name));
  }

  // Calls base::win::ValidateShortcut() with expected properties for
  // |profile_name|'s shortcut.
  void ValidateProfileShortcut(const string16& profile_name) {
    EXPECT_TRUE(ProfileShortcutExistsAtDefaultPath(profile_name));

    FilePath shortcut_path = GetDefaultShortcutPathForProfile(profile_name);

    // TODO(asvitkine): With this new struct method for VerifyShortcut you can
    // now test more properties like: arguments, icon, icon_index, and app_id.
    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(exe_path_);
    expected_properties.set_description(distribution_->GetAppDescription());
    expected_properties.set_dual_mode(false);
    base::win::ValidateShortcut(shortcut_path, expected_properties);
  }

  BrowserDistribution* distribution_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;
  ProfileInfoCache* profile_info_cache_;
  base::ScopedPathOverride fake_user_desktop_;
  FilePath exe_path_;
  FilePath dest_path_;
  FilePath shortcuts_directory_;
  string16 profile_name_;
  FilePath second_dest_path_;
  string16 second_profile_name_;
};

TEST_F(ProfileShortcutManagerTest, ShortcutFilename) {
  const string16 kProfileName = L"Harry";
  const string16 expected_name = kProfileName + L" - " +
      distribution_->GetAppShortCutName() + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName,
                                                              distribution_));
}

TEST_F(ProfileShortcutManagerTest, UnbadgedShortcutFilename) {
  EXPECT_EQ(distribution_->GetAppShortCutName() + installer::kLnkExt,
            profiles::internal::GetShortcutFilenameForProfile(string16(),
                                                              distribution_));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreate) {
  ProfileShortcutManagerTest::SetupDefaultProfileShortcut();

  profile_info_cache_->AddProfileToCache(second_dest_path_,
                                         second_profile_name_, string16(), 0);
  profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
  RunPendingTasks();

  // We now have 2 profiles, so we expect a new shortcut with profile
  // information for this 2nd profile.
  ValidateProfileShortcut(second_profile_name_);
  ASSERT_TRUE(file_util::PathExists(second_dest_path_.Append(
      FILE_PATH_LITERAL("Google Profile.ico"))));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsUpdate) {
  ProfileShortcutManagerTest::SetupDefaultProfileShortcut();

  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));

  profile_info_cache_->AddProfileToCache(second_dest_path_,
                                         second_profile_name_, string16(), 0);
  profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
  RunPendingTasks();
  ValidateProfileShortcut(second_profile_name_);

  // Cause an update in ProfileShortcutManager by modifying the profile info
  // cache.
  const string16 new_profile_name = L"New Profile Name";
  profile_info_cache_->SetNameOfProfileAtIndex(
      profile_info_cache_->GetIndexOfProfileWithPath(second_dest_path_),
      new_profile_name);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));
  ValidateProfileShortcut(new_profile_name);
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsDeleteSecondToLast) {
  ProfileShortcutManagerTest::SetupAndCreateTwoShortcuts();

  // Delete one shortcut.
  profile_info_cache_->DeleteProfileFromCache(second_dest_path_);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));

  // Verify that the profile name has been removed from the remaining shortcut.
  ValidateProfileShortcut(string16());
  // Verify that an additional shortcut, with the default profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name_));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreateSecond) {
  ProfileShortcutManagerTest::SetupAndCreateTwoShortcuts();

  // Delete one shortcut.
  profile_info_cache_->DeleteProfileFromCache(second_dest_path_);
  RunPendingTasks();

  // Verify that a default shortcut exists (no profile name/avatar).
  ValidateProfileShortcut(string16());
  // Verify that an additional shortcut, with the default profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name_));

  // Create a second profile and shortcut.
  profile_info_cache_->AddProfileToCache(second_dest_path_,
                                         second_profile_name_, string16(), 0);
  profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
  RunPendingTasks();
  ValidateProfileShortcut(second_profile_name_);

  // Verify that the original shortcut received the profile's name.
  ValidateProfileShortcut(profile_name_);
  // Verify that a default shortcut no longer exists.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(string16()));
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcuts) {
  ProfileShortcutManagerTest::SetupAndCreateTwoShortcuts();

  const FilePath old_shortcut_path =
      GetDefaultShortcutPathForProfile(second_profile_name_);
  const FilePath new_shortcut_path =
      shortcuts_directory_.Append(L"MyChrome.lnk");
  ASSERT_TRUE(file_util::Move(old_shortcut_path, new_shortcut_path));

  // Ensure that a new shortcut does not get made if the old one was renamed.
  profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));
  EXPECT_TRUE(file_util::PathExists(new_shortcut_path));

  // Delete the renamed shortcut and try to create it again, which should work.
  ASSERT_TRUE(file_util::Delete(new_shortcut_path, false));
  EXPECT_FALSE(file_util::PathExists(new_shortcut_path));
  profile_shortcut_manager_->CreateProfileShortcut(second_dest_path_);
  RunPendingTasks();
  EXPECT_TRUE(ProfileShortcutExistsAtDefaultPath(second_profile_name_));
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsGetDeleted) {
  ProfileShortcutManagerTest::SetupAndCreateTwoShortcuts();

  const FilePath old_shortcut_path =
      GetDefaultShortcutPathForProfile(second_profile_name_);
  const FilePath new_shortcut_path =
      shortcuts_directory_.Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(file_util::CopyFile(old_shortcut_path, new_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(old_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(new_shortcut_path));

  // Also, copy the shortcut for the first user and ensure it gets preserved.
  const FilePath preserved_shortcut_path =
      shortcuts_directory_.Append(L"Preserved.lnk");
  ASSERT_TRUE(file_util::CopyFile(
      GetDefaultShortcutPathForProfile(profile_name_),
      preserved_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(preserved_shortcut_path));

  // Delete the profile and ensure both shortcuts were also deleted.
  profile_info_cache_->DeleteProfileFromCache(second_dest_path_);
  RunPendingTasks();
  EXPECT_FALSE(file_util::PathExists(old_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(new_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(preserved_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsAfterProfileRename) {
  ProfileShortcutManagerTest::SetupAndCreateTwoShortcuts();

  const FilePath old_shortcut_path =
      GetDefaultShortcutPathForProfile(second_profile_name_);
  const FilePath new_shortcut_path =
      shortcuts_directory_.Append(L"MyChrome.lnk");

  // Make a copy of the shortcut.
  ASSERT_TRUE(file_util::CopyFile(old_shortcut_path, new_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(old_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(new_shortcut_path));

  // Now, rename the profile.
  const string16 new_profile_name = L"New profile";
  ASSERT_NE(second_profile_name_, new_profile_name);
  profile_info_cache_->SetNameOfProfileAtIndex(
      profile_info_cache_->GetIndexOfProfileWithPath(second_dest_path_),
      new_profile_name);
  RunPendingTasks();

  // The original shortcut should be renamed but the copied shortcut should
  // keep its name.
  EXPECT_FALSE(file_util::PathExists(old_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(new_shortcut_path));
  const FilePath updated_shortcut_path =
      GetDefaultShortcutPathForProfile(new_profile_name);
  EXPECT_TRUE(file_util::PathExists(updated_shortcut_path));
}

