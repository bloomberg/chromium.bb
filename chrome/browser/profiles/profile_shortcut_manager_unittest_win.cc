// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>  // For CoInitialize().

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/location.h"
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
#include "chrome/installer/util/product.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/chromium_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

class ProfileShortcutManagerTest : public testing::Test {
 protected:
  ProfileShortcutManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_shortcut_manager_(NULL),
        profile_info_cache_(NULL),
        fake_user_desktop_(base::DIR_USER_DESKTOP),
        fake_system_desktop_(base::DIR_COMMON_DESKTOP) {
  }

  virtual void SetUp() OVERRIDE {
    CoInitialize(NULL);

    TestingBrowserProcess* browser_process =
        TestingBrowserProcess::GetGlobal();
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_info_cache_ = profile_manager_->profile_info_cache();
    profile_shortcut_manager_.reset(
        ProfileShortcutManager::Create(profile_manager_->profile_manager()));
    profile_1_name_ = L"My profile";
    profile_1_path_ = CreateProfileDirectory(profile_1_name_);
    profile_2_name_ = L"My profile 2";
    profile_2_path_ = CreateProfileDirectory(profile_2_name_);
    profile_3_name_ = L"My profile 3";
    profile_3_path_ = CreateProfileDirectory(profile_3_name_);
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunUntilIdle();

    // Delete all profiles and ensure their shortcuts got removed.
    const int num_profiles = profile_info_cache_->GetNumberOfProfiles();
    for (int i = 0; i < num_profiles; ++i) {
      const base::FilePath profile_path =
          profile_info_cache_->GetPathOfProfileAtIndex(0);
      string16 profile_name = profile_info_cache_->GetNameOfProfileAtIndex(0);
      profile_info_cache_->DeleteProfileFromCache(profile_path);
      RunPendingTasks();
      ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name));
      const base::FilePath icon_path =
          profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
      ASSERT_FALSE(file_util::PathExists(icon_path));
    }
  }

  base::FilePath CreateProfileDirectory(const string16& profile_name) {
    const base::FilePath profile_path =
        profile_info_cache_->GetUserDataDir().Append(profile_name);
    file_util::CreateDirectoryW(profile_path);
    return profile_path;
  }

  void RunPendingTasks() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  void SetupDefaultProfileShortcut(const tracked_objects::Location& location) {
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_))
        << location.ToString();
    // A non-badged shortcut for chrome is automatically created with the
    // first profile (for the case when the user deletes their only profile).
    profile_info_cache_->AddProfileToCache(profile_1_path_, profile_1_name_,
                                           string16(), 0, false);
    RunPendingTasks();
    // We now have 1 profile, so we expect a new shortcut with no profile
    // information.
    ValidateNonProfileShortcut(location);
  }

  void SetupAndCreateTwoShortcuts(const tracked_objects::Location& location) {
    ASSERT_EQ(0, profile_info_cache_->GetNumberOfProfiles())
        << location.ToString();
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_))
        << location.ToString();

    profile_info_cache_->AddProfileToCache(profile_1_path_, profile_1_name_,
                                           string16(), 0, false);
    CreateProfileWithShortcut(location, profile_2_name_, profile_2_path_);
    ValidateProfileShortcut(location, profile_1_name_, profile_1_path_);
  }

  // Returns the default shortcut path for this profile.
  base::FilePath GetDefaultShortcutPathForProfile(
      const string16& profile_name) {
    return GetUserShortcutsDirectory().Append(
        profiles::internal::GetShortcutFilenameForProfile(profile_name,
                                                          GetDistribution()));
  }

  // Returns true if the shortcut for this profile exists.
  bool ProfileShortcutExistsAtDefaultPath(const string16& profile_name) {
    return file_util::PathExists(
        GetDefaultShortcutPathForProfile(profile_name));
  }

  // Calls base::win::ValidateShortcut() with expected properties for the
  // shortcut at |shortcut_path| for the profile at |profile_path|.
  void ValidateProfileShortcutAtPath(const tracked_objects::Location& location,
                                     const base::FilePath& shortcut_path,
                                     const base::FilePath& profile_path) {
    EXPECT_TRUE(file_util::PathExists(shortcut_path)) << location.ToString();

    // Ensure that the corresponding icon exists.
    const base::FilePath icon_path =
        profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
    EXPECT_TRUE(file_util::PathExists(icon_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(GetExePath());
    expected_properties.set_description(GetDistribution()->GetAppDescription());
    expected_properties.set_dual_mode(false);
    expected_properties.set_arguments(
        profiles::internal::CreateProfileShortcutFlags(profile_path));
    expected_properties.set_icon(icon_path, 0);
    base::win::ValidateShortcut(shortcut_path, expected_properties);
  }

  // Calls base::win::ValidateShortcut() with expected properties for
  // |profile_name|'s shortcut.
  void ValidateProfileShortcut(const tracked_objects::Location& location,
                               const string16& profile_name,
                               const base::FilePath& profile_path) {
    ValidateProfileShortcutAtPath(
        location, GetDefaultShortcutPathForProfile(profile_name), profile_path);
  }

  void ValidateNonProfileShortcutAtPath(
      const tracked_objects::Location& location,
      const base::FilePath& shortcut_path) {
    EXPECT_TRUE(file_util::PathExists(shortcut_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(GetExePath());
    expected_properties.set_arguments(string16());
    expected_properties.set_icon(GetExePath(), 0);
    expected_properties.set_description(GetDistribution()->GetAppDescription());
    expected_properties.set_dual_mode(false);
    base::win::ValidateShortcut(shortcut_path, expected_properties);
  }

  void ValidateNonProfileShortcut(const tracked_objects::Location& location) {
    const base::FilePath shortcut_path =
        GetDefaultShortcutPathForProfile(string16());
    ValidateNonProfileShortcutAtPath(location, shortcut_path);
  }

  void CreateProfileWithShortcut(const tracked_objects::Location& location,
                                 const string16& profile_name,
                                 const base::FilePath& profile_path) {
    ASSERT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_name))
        << location.ToString();
    profile_info_cache_->AddProfileToCache(profile_path, profile_name,
                                           string16(), 0, false);
    profile_shortcut_manager_->CreateProfileShortcut(profile_path);
    RunPendingTasks();
    ValidateProfileShortcut(location, profile_name, profile_path);
  }

  // Creates a regular (non-profile) desktop shortcut with the given name and
  // returns its path. Fails the test if an error occurs.
  base::FilePath CreateRegularShortcutWithName(
      const tracked_objects::Location& location,
      const string16& shortcut_name) {
    const base::FilePath shortcut_path =
        GetUserShortcutsDirectory().Append(shortcut_name + installer::kLnkExt);
    EXPECT_FALSE(file_util::PathExists(shortcut_path)) << location.ToString();

    installer::Product product(GetDistribution());
    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    product.AddDefaultShortcutProperties(GetExePath(), &properties);
    properties.set_shortcut_name(shortcut_name);
    EXPECT_TRUE(ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, GetDistribution(), properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS)) << location.ToString();
    EXPECT_TRUE(file_util::PathExists(shortcut_path)) << location.ToString();

    return shortcut_path;
  }

  base::FilePath CreateRegularSystemLevelShortcut(
      const tracked_objects::Location& location) {
    BrowserDistribution* distribution = GetDistribution();
    installer::Product product(distribution);
    ShellUtil::ShortcutProperties properties(ShellUtil::SYSTEM_LEVEL);
    product.AddDefaultShortcutProperties(GetExePath(), &properties);
    EXPECT_TRUE(ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, distribution, properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS)) << location.ToString();
    const base::FilePath system_level_shortcut_path =
        GetSystemShortcutsDirectory().Append(
            distribution->GetAppShortCutName() + installer::kLnkExt);
    EXPECT_TRUE(file_util::PathExists(system_level_shortcut_path))
        << location.ToString();
    return system_level_shortcut_path;
  }

  void RenameProfile(const tracked_objects::Location& location,
                     const base::FilePath& profile_path,
                     const string16& new_profile_name) {
    const size_t profile_index =
        profile_info_cache_->GetIndexOfProfileWithPath(profile_2_path_);
    ASSERT_NE(std::string::npos, profile_index);
    ASSERT_NE(profile_info_cache_->GetNameOfProfileAtIndex(profile_index),
              new_profile_name);
    profile_info_cache_->SetNameOfProfileAtIndex(profile_index,
                                                 new_profile_name);
    RunPendingTasks();
  }

  BrowserDistribution* GetDistribution() {
    return BrowserDistribution::GetDistribution();
  }

  base::FilePath GetExePath() {
    base::FilePath exe_path;
    EXPECT_TRUE(PathService::Get(base::FILE_EXE, &exe_path));
    return exe_path;
  }

  base::FilePath GetUserShortcutsDirectory() {
    base::FilePath user_shortcuts_directory;
    EXPECT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           GetDistribution(),
                                           ShellUtil::CURRENT_USER,
                                           &user_shortcuts_directory));
    return user_shortcuts_directory;
  }

  base::FilePath GetSystemShortcutsDirectory() {
    base::FilePath system_shortcuts_directory;
    EXPECT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           GetDistribution(),
                                           ShellUtil::SYSTEM_LEVEL,
                                           &system_shortcuts_directory));
    return system_shortcuts_directory;
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;
  ProfileInfoCache* profile_info_cache_;
  base::ScopedPathOverride fake_user_desktop_;
  base::ScopedPathOverride fake_system_desktop_;
  string16 profile_1_name_;
  base::FilePath profile_1_path_;
  string16 profile_2_name_;
  base::FilePath profile_2_path_;
  string16 profile_3_name_;
  base::FilePath profile_3_path_;
};

TEST_F(ProfileShortcutManagerTest, ShortcutFilename) {
  const string16 kProfileName = L"Harry";
  BrowserDistribution* distribution = GetDistribution();
  const string16 expected_name = kProfileName + L" - " +
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME) + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName,
                                                              distribution));
}

TEST_F(ProfileShortcutManagerTest, ShortcutLongFilenameIsTrimmed) {
  const string16 kLongProfileName = L"Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry";
  const string16 file_name =
      profiles::internal::GetShortcutFilenameForProfile(kLongProfileName,
                                                        GetDistribution());
  EXPECT_LT(file_name.size(), kLongProfileName.size());
}

TEST_F(ProfileShortcutManagerTest, ShortcutFilenameStripsReservedCharacters) {
  const string16 kProfileName = L"<Harry/>";
  const string16 kSanitizedProfileName = L"Harry";
  BrowserDistribution* distribution = GetDistribution();
  const string16 expected_name = kSanitizedProfileName + L" - " +
      distribution->GetAppShortCutName() + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName,
                                                              distribution));
}

TEST_F(ProfileShortcutManagerTest, UnbadgedShortcutFilename) {
  BrowserDistribution* distribution = GetDistribution();
  EXPECT_EQ(distribution->GetAppShortCutName() + installer::kLnkExt,
            profiles::internal::GetShortcutFilenameForProfile(string16(),
                                                              distribution));
}

TEST_F(ProfileShortcutManagerTest, ShortcutFlags) {
  const string16 kProfileName = L"MyProfileX";
  const base::FilePath profile_path =
      profile_info_cache_->GetUserDataDir().Append(kProfileName);
  EXPECT_EQ(L"--profile-directory=\"" + kProfileName + L"\"",
            profiles::internal::CreateProfileShortcutFlags(profile_path));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreate) {
  SetupDefaultProfileShortcut(FROM_HERE);
  // Validation is done by |ValidateProfileShortcutAtPath()| which is called
  // by |CreateProfileWithShortcut()|.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsUpdate) {
  SetupDefaultProfileShortcut(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Cause an update in ProfileShortcutManager by modifying the profile info
  // cache.
  const string16 new_profile_2_name = L"New Profile Name";
  RenameProfile(FROM_HERE, profile_2_path_, new_profile_2_name);
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, CreateSecondProfileBadgesFirstShortcut) {
  SetupDefaultProfileShortcut(FROM_HERE);
  // Assert that a shortcut without a profile name exists.
  ASSERT_TRUE(ProfileShortcutExistsAtDefaultPath(string16()));

  // Create a second profile without a shortcut.
  profile_info_cache_->AddProfileToCache(profile_2_path_, profile_2_name_,
                                         string16(), 0, false);
  RunPendingTasks();

  // Ensure that the second profile doesn't have a shortcut and that the first
  // profile's shortcut got renamed and badged.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(string16()));
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsDeleteSecondToLast) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  // Delete one shortcut.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));

  // Verify that the profile name has been removed from the remaining shortcut.
  ValidateNonProfileShortcut(FROM_HERE);
  // Verify that an additional shortcut, with the default profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));
}

TEST_F(ProfileShortcutManagerTest, DeleteSecondToLastProfileWithoutShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(file_util::Delete(profile_1_shortcut_path, false));
  ASSERT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(file_util::PathExists(profile_2_shortcut_path));

  // Delete the profile that doesn't have a shortcut.
  profile_info_cache_->DeleteProfileFromCache(profile_1_path_);
  RunPendingTasks();

  // Verify that the remaining shortcut does not have a profile name.
  ValidateNonProfileShortcut(FROM_HERE);
  // Verify that shortcuts with profile names do not exist.
  EXPECT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DeleteSecondToLastProfileWithShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(file_util::Delete(profile_1_shortcut_path, false));
  ASSERT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(file_util::PathExists(profile_2_shortcut_path));

  // Delete the profile that has a shortcut.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();

  // Verify that the remaining shortcut does not have a profile name.
  ValidateNonProfileShortcut(FROM_HERE);
  // Verify that shortcuts with profile names do not exist.
  EXPECT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DeleteOnlyProfileWithShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_3_name_, profile_3_path_);

  const base::FilePath non_profile_shortcut_path =
      GetDefaultShortcutPathForProfile(string16());
  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_3_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_3_name_);

  // Delete shortcuts for the first two profiles.
  ASSERT_TRUE(file_util::Delete(profile_1_shortcut_path, false));
  ASSERT_TRUE(file_util::Delete(profile_2_shortcut_path, false));

  // Only the shortcut to the third profile should exist.
  ASSERT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  ASSERT_FALSE(file_util::PathExists(profile_2_shortcut_path));
  ASSERT_FALSE(file_util::PathExists(non_profile_shortcut_path));
  ASSERT_TRUE(file_util::PathExists(profile_3_shortcut_path));

  // Delete the third profile and check that its shortcut is gone and no
  // shortcuts have been re-created.
  profile_info_cache_->DeleteProfileFromCache(profile_3_path_);
  RunPendingTasks();
  ASSERT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  ASSERT_FALSE(file_util::PathExists(profile_2_shortcut_path));
  ASSERT_FALSE(file_util::PathExists(profile_3_shortcut_path));
  ASSERT_FALSE(file_util::PathExists(non_profile_shortcut_path));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreateSecond) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  // Delete one shortcut.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();

  // Verify that a default shortcut exists (no profile name/avatar).
  ValidateNonProfileShortcut(FROM_HERE);
  // Verify that an additional shortcut, with the first profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));

  // Create a second profile and shortcut.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);

  // Verify that the original shortcut received the profile's name.
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
  // Verify that a default shortcut no longer exists.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(string16()));
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  ASSERT_TRUE(file_util::Move(profile_2_shortcut_path_1,
                              profile_2_shortcut_path_2));

  // Ensure that a new shortcut does not get made if the old one was renamed.
  profile_shortcut_manager_->CreateProfileShortcut(profile_2_path_);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Delete the renamed shortcut and try to create it again, which should work.
  ASSERT_TRUE(file_util::Delete(profile_2_shortcut_path_2, false));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_2));
  profile_shortcut_manager_->CreateProfileShortcut(profile_2_path_);
  RunPendingTasks();
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsGetDeleted) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(file_util::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Also, copy the shortcut for the first user and ensure it gets preserved.
  const base::FilePath preserved_profile_1_shortcut_path =
      GetUserShortcutsDirectory().Append(L"Preserved.lnk");
  ASSERT_TRUE(file_util::CopyFile(
      GetDefaultShortcutPathForProfile(profile_1_name_),
      preserved_profile_1_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(preserved_profile_1_shortcut_path));

  // Delete the profile and ensure both shortcuts were also deleted.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_1));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_2));
  ValidateNonProfileShortcutAtPath(FROM_HERE,
                                   preserved_profile_1_shortcut_path);
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsAfterProfileRename) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(file_util::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Now, rename the profile.
  const string16 new_profile_2_name = L"New profile";
  RenameProfile(FROM_HERE, profile_2_path_, new_profile_2_name);

  // The original shortcut should be renamed but the copied shortcut should
  // keep its name.
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_1));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, UpdateShortcutWithNoFlags) {
  SetupDefaultProfileShortcut(FROM_HERE);

  // Delete the shortcut that got created for this profile and instead make
  // a new one without any command-line flags.
  ASSERT_TRUE(file_util::Delete(GetDefaultShortcutPathForProfile(string16()),
                                false));
  const base::FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE,
                                    GetDistribution()->GetAppShortCutName());

  // Add another profile and check that the shortcut was replaced with
  // a badged shortcut with the right command line for the profile
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_FALSE(file_util::PathExists(regular_shortcut_path));
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, UpdateTwoShortcutsWithNoFlags) {
  SetupDefaultProfileShortcut(FROM_HERE);

  // Delete the shortcut that got created for this profile and instead make
  // two new ones without any command-line flags.
  ASSERT_TRUE(file_util::Delete(GetDefaultShortcutPathForProfile(string16()),
                                false));
  const base::FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE,
                                    GetDistribution()->GetAppShortCutName());
  const base::FilePath customized_regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE, L"MyChrome");

  // Add another profile and check that one shortcut was renamed and that the
  // other shortcut was updated but kept the same name.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_FALSE(file_util::PathExists(regular_shortcut_path));
  ValidateProfileShortcutAtPath(FROM_HERE, customized_regular_shortcut_path,
                                profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, RemoveProfileShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);
  CreateProfileWithShortcut(FROM_HERE, profile_3_name_, profile_3_path_);

  const base::FilePath profile_1_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Make copies of the shortcuts for both profiles.
  const base::FilePath profile_1_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"Copied1.lnk");
  const base::FilePath profile_2_shortcut_path_2 =
      GetUserShortcutsDirectory().Append(L"Copied2.lnk");
  ASSERT_TRUE(file_util::CopyFile(profile_1_shortcut_path_1,
                                  profile_1_shortcut_path_2));
  ASSERT_TRUE(file_util::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_1_shortcut_path_2,
                                profile_1_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Delete shortcuts for profile 1 and ensure that they got deleted while the
  // shortcuts for profile 2 were kept.
  profile_shortcut_manager_->RemoveProfileShortcuts(profile_1_path_);
  RunPendingTasks();
  EXPECT_FALSE(file_util::PathExists(profile_1_shortcut_path_1));
  EXPECT_FALSE(file_util::PathExists(profile_1_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest, HasProfileShortcuts) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  struct HasShortcutsResult {
    bool has_shortcuts;
    void set_has_shortcuts(bool value) { has_shortcuts = value; }
  } result = { false };

  const base::Callback<void(bool)> callback =
      base::Bind(&HasShortcutsResult::set_has_shortcuts,
                 base::Unretained(&result));

  // Profile 2 should have a shortcut initially.
  profile_shortcut_manager_->HasProfileShortcuts(profile_2_path_, callback);
  RunPendingTasks();
  EXPECT_TRUE(result.has_shortcuts);

  // Delete the shortcut and check that the function returns false.
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  ASSERT_TRUE(file_util::Delete(profile_2_shortcut_path, false));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path));
  profile_shortcut_manager_->HasProfileShortcuts(profile_2_path_, callback);
  RunPendingTasks();
  EXPECT_FALSE(result.has_shortcuts);
}

TEST_F(ProfileShortcutManagerTest, ProfileShortcutsWithSystemLevelShortcut) {
  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Create the initial profile.
  profile_info_cache_->AddProfileToCache(profile_1_path_, profile_1_name_,
                                         string16(), 0, false);
  RunPendingTasks();
  ASSERT_EQ(1U, profile_info_cache_->GetNumberOfProfiles());

  // Ensure system-level continues to exist and user-level was not created.
  EXPECT_TRUE(file_util::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(
                   GetDefaultShortcutPathForProfile(string16())));

  // Create another profile with a shortcut and ensure both profiles receive
  // user-level profile shortcuts and the system-level one still exists.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_TRUE(file_util::PathExists(system_level_shortcut_path));

  // Create a third profile without a shortcut and ensure it doesn't get one.
  profile_info_cache_->AddProfileToCache(profile_3_path_, profile_3_name_,
                                         string16(), 0, false);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));

  // Ensure that changing the avatar icon and the name does not result in a
  // shortcut being created.
  profile_info_cache_->SetAvatarIconOfProfileAtIndex(
      profile_info_cache_->GetIndexOfProfileWithPath(profile_3_path_), 3);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));

  const string16 new_profile_3_name = L"New Name 3";
  profile_info_cache_->SetNameOfProfileAtIndex(
      profile_info_cache_->GetIndexOfProfileWithPath(profile_3_path_),
      new_profile_3_name);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_3_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(new_profile_3_name));

  // Rename the second profile and ensure its shortcut got renamed.
  const string16 new_profile_2_name = L"New Name 2";
  profile_info_cache_->SetNameOfProfileAtIndex(
      profile_info_cache_->GetIndexOfProfileWithPath(profile_2_path_),
      new_profile_2_name);
  RunPendingTasks();
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
  ValidateProfileShortcut(FROM_HERE, new_profile_2_name, profile_2_path_);
}

TEST_F(ProfileShortcutManagerTest,
       DeleteSecondToLastProfileWithSystemLevelShortcut) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Delete a profile and verify that only the system-level shortcut still
  // exists.
  profile_info_cache_->DeleteProfileFromCache(profile_1_path_);
  RunPendingTasks();

  EXPECT_TRUE(file_util::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(string16()));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_2_name_));
}

TEST_F(ProfileShortcutManagerTest,
       DeleteSecondToLastProfileWithShortcutWhenSystemLevelShortcutExists) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const base::FilePath profile_1_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_1_name_);
  const base::FilePath profile_2_shortcut_path =
      GetDefaultShortcutPathForProfile(profile_2_name_);

  // Delete the shortcut for the first profile, but keep the one for the 2nd.
  ASSERT_TRUE(file_util::Delete(profile_1_shortcut_path, false));
  ASSERT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  ASSERT_TRUE(file_util::PathExists(profile_2_shortcut_path));

  const base::FilePath system_level_shortcut_path =
      CreateRegularSystemLevelShortcut(FROM_HERE);

  // Delete the profile that has a shortcut, which will exercise the non-profile
  // shortcut creation path in |DeleteDesktopShortcutsAndIconFile()|, which is
  // not covered by the |DeleteSecondToLastProfileWithSystemLevelShortcut| test.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();

  // Verify that only the system-level shortcut still exists.
  EXPECT_TRUE(file_util::PathExists(system_level_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(
                   GetDefaultShortcutPathForProfile(string16())));
  EXPECT_FALSE(file_util::PathExists(profile_1_shortcut_path));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path));
}
