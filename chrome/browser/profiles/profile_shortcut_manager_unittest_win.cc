// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>  // For CoInitialize().

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
        TestingBrowserProcess::GetGlobal();
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_info_cache_ = profile_manager_->profile_info_cache();
    profile_shortcut_manager_.reset(
        ProfileShortcutManager::Create(profile_manager_->profile_manager()));

    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &exe_path_));
    ASSERT_TRUE(ShellUtil::GetShortcutPath(ShellUtil::SHORTCUT_LOCATION_DESKTOP,
                                           distribution_,
                                           ShellUtil::CURRENT_USER,
                                           &shortcuts_directory_));

    profile_1_name_ = L"My profile";
    profile_1_path_ = CreateProfileDirectory(profile_1_name_);
    profile_2_name_ = L"My profile 2";
    profile_2_path_ = CreateProfileDirectory(profile_2_name_);
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
      const FilePath icon_path =
          profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
      ASSERT_FALSE(file_util::PathExists(icon_path));
    }
  }

  FilePath CreateProfileDirectory(const string16& profile_name) {
    const FilePath profile_path =
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
    ValidateProfileShortcut(location, string16(), profile_1_path_);
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

  // Calls base::win::ValidateShortcut() with expected properties for the
  // shortcut at |shortcut_path| for the profile at |profile_path|.
  void ValidateProfileShortcutAtPath(const tracked_objects::Location& location,
                                     const FilePath& shortcut_path,
                                     const FilePath& profile_path) {
    EXPECT_TRUE(file_util::PathExists(shortcut_path)) << location.ToString();

    // Ensure that the corresponding icon exists.
    const FilePath icon_path =
        profile_path.AppendASCII(profiles::internal::kProfileIconFileName);
    EXPECT_TRUE(file_util::PathExists(icon_path)) << location.ToString();

    base::win::ShortcutProperties expected_properties;
    expected_properties.set_target(exe_path_);
    expected_properties.set_description(distribution_->GetAppDescription());
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
                               const FilePath& profile_path) {
    ValidateProfileShortcutAtPath(
        location, GetDefaultShortcutPathForProfile(profile_name), profile_path);
  }

  void CreateProfileWithShortcut(const tracked_objects::Location& location,
                                 const string16& profile_name,
                                 const FilePath& profile_path) {
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
  FilePath CreateRegularShortcutWithName(
      const tracked_objects::Location& location,
      const string16& shortcut_name) {
    const FilePath shortcut_path =
        shortcuts_directory_.Append(shortcut_name + installer::kLnkExt);
    EXPECT_FALSE(file_util::PathExists(shortcut_path)) << location.ToString();

    installer::Product product(distribution_);
    ShellUtil::ShortcutProperties properties(ShellUtil::CURRENT_USER);
    product.AddDefaultShortcutProperties(exe_path_, &properties);
    properties.set_shortcut_name(shortcut_name);
    EXPECT_TRUE(ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_DESKTOP, distribution_, properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS)) << location.ToString();
    EXPECT_TRUE(file_util::PathExists(shortcut_path)) << location.ToString();

    return shortcut_path;
  }

  void RenameProfile(const tracked_objects::Location& location,
                     const FilePath& profile_path,
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

  BrowserDistribution* distribution_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;
  ProfileInfoCache* profile_info_cache_;
  base::ScopedPathOverride fake_user_desktop_;
  FilePath exe_path_;
  FilePath shortcuts_directory_;
  string16 profile_1_name_;
  FilePath profile_1_path_;
  string16 profile_2_name_;
  FilePath profile_2_path_;
};

TEST_F(ProfileShortcutManagerTest, ShortcutFilename) {
  const string16 kProfileName = L"Harry";
  const string16 expected_name = kProfileName + L" - " +
      distribution_->GetAppShortCutName() + installer::kLnkExt;
  EXPECT_EQ(expected_name,
            profiles::internal::GetShortcutFilenameForProfile(kProfileName,
                                                              distribution_));
}

TEST_F(ProfileShortcutManagerTest, ShortcutLongFilenameIsTrimmed) {
  const string16 kLongProfileName = L"Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry"
      L"Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry Harry";
  string16 file_name =
      profiles::internal::GetShortcutFilenameForProfile(
          kLongProfileName, distribution_);
  EXPECT_LT(file_name.size(), kLongProfileName.size());
}

TEST_F(ProfileShortcutManagerTest, ShortcutFilenameStripsReservedCharacters) {
  const string16 kProfileName = L"<Harry/>";
  const string16 kSanitizedProfileName = L"Harry";
  const string16 expected_name = kSanitizedProfileName + L" - " +
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

TEST_F(ProfileShortcutManagerTest, ShortcutFlags) {
  const string16 kProfileName = L"MyProfileX";
  const FilePath profile_path =
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
  ValidateProfileShortcut(FROM_HERE, string16(), profile_1_path_);
  // Verify that an additional shortcut, with the default profile's name does
  // not exist.
  EXPECT_FALSE(ProfileShortcutExistsAtDefaultPath(profile_1_name_));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreateSecond) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  // Delete one shortcut.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();

  // Verify that a default shortcut exists (no profile name/avatar).
  ValidateProfileShortcut(FROM_HERE, string16(), profile_1_path_);
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

  const FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const FilePath profile_2_shortcut_path_2 =
      shortcuts_directory_.Append(L"MyChrome.lnk");
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

  const FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const FilePath profile_2_shortcut_path_2 =
      shortcuts_directory_.Append(L"MyChrome.lnk");
  // Make a copy of the shortcut.
  ASSERT_TRUE(file_util::CopyFile(profile_2_shortcut_path_1,
                                  profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_1,
                                profile_2_path_);
  ValidateProfileShortcutAtPath(FROM_HERE, profile_2_shortcut_path_2,
                                profile_2_path_);

  // Also, copy the shortcut for the first user and ensure it gets preserved.
  const FilePath preserved_profile_1_shortcut_path =
      shortcuts_directory_.Append(L"Preserved.lnk");
  ASSERT_TRUE(file_util::CopyFile(
      GetDefaultShortcutPathForProfile(profile_1_name_),
      preserved_profile_1_shortcut_path));
  EXPECT_TRUE(file_util::PathExists(preserved_profile_1_shortcut_path));

  // Delete the profile and ensure both shortcuts were also deleted.
  profile_info_cache_->DeleteProfileFromCache(profile_2_path_);
  RunPendingTasks();
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_1));
  EXPECT_FALSE(file_util::PathExists(profile_2_shortcut_path_2));
  ValidateProfileShortcutAtPath(FROM_HERE, preserved_profile_1_shortcut_path,
                                profile_1_path_);
}

TEST_F(ProfileShortcutManagerTest, RenamedDesktopShortcutsAfterProfileRename) {
  SetupAndCreateTwoShortcuts(FROM_HERE);

  const FilePath profile_2_shortcut_path_1 =
      GetDefaultShortcutPathForProfile(profile_2_name_);
  const FilePath profile_2_shortcut_path_2 =
      shortcuts_directory_.Append(L"MyChrome.lnk");
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
  const FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE,
                                    distribution_->GetAppShortCutName());

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
  const FilePath regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE,
                                    distribution_->GetAppShortCutName());
  const FilePath customized_regular_shortcut_path =
      CreateRegularShortcutWithName(FROM_HERE, L"MyChrome");

  // Add another profile and check that one shortcut was renamed and that the
  // other shortcut was updated but kept the same name.
  CreateProfileWithShortcut(FROM_HERE, profile_2_name_, profile_2_path_);
  EXPECT_FALSE(file_util::PathExists(regular_shortcut_path));
  ValidateProfileShortcutAtPath(FROM_HERE, customized_regular_shortcut_path,
                                profile_1_path_);
  ValidateProfileShortcut(FROM_HERE, profile_1_name_, profile_1_path_);
}
