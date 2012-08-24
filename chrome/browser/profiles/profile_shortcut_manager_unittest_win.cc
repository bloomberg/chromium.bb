// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_shortcut_manager.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace {

ShellUtil::VerifyShortcutStatus VerifyProfileShortcut(
    const string16& profile_name) {
  FilePath exe_path;
  CHECK(PathService::Get(base::FILE_EXE, &exe_path));

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Get the desktop path of the current user.
  FilePath shortcut_path;
  ShellUtil::GetDesktopPath(false, &shortcut_path);
  // Get the name of the shortcut with profile attached
  string16 shortcut_name;
  ShellUtil::GetChromeShortcutName(dist, false, profile_name,
                                   &shortcut_name);
  shortcut_path = shortcut_path.Append(shortcut_name);

  return ShellUtil::VerifyChromeShortcut(
          exe_path.value(), shortcut_path.value(), dist->GetAppDescription(),
          0);
}

}  // namespace

class ProfileShortcutManagerTest : public testing::Test {
 protected:
  ProfileShortcutManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() {
    TestingBrowserProcess* browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    profile_manager_.reset(new TestingProfileManager(browser_process));
    ASSERT_TRUE(profile_manager_->SetUp());
    ProfileInfoCache* cache = profile_manager_->profile_info_cache();
    // Profile shortcut manager will be NULL for non-windows platforms
    profile_shortcut_manager_.reset(ProfileShortcutManager::Create(*cache));

    dest_path_ = profile_manager_->profile_info_cache()->GetUserDataDir();
    dest_path_ = dest_path_.Append(FILE_PATH_LITERAL("New Profile 1"));
    file_util::CreateDirectoryW(dest_path_);
    profile_name_ = ASCIIToUTF16("My profile");
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
    profile_shortcut_manager_->DeleteProfileDesktopShortcut(
      dest_path_, profile_name_);
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
    EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
              VerifyProfileShortcut(profile_name_));
    ASSERT_FALSE(file_util::PathExists(dest_path_.Append(
        FILE_PATH_LITERAL("Google Profile.ico"))));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<ProfileShortcutManager> profile_shortcut_manager_;
  FilePath dest_path_;
  string16 profile_name_;
};

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsCreate) {
  if (!profile_shortcut_manager_.get())
    return;
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
            VerifyProfileShortcut(profile_name_));
  ASSERT_FALSE(file_util::PathExists(dest_path_.Append(
      FILE_PATH_LITERAL("Google Profile.ico"))));

  gfx::Image& avatar = ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_PROFILE_AVATAR_0);

  profile_shortcut_manager_->StartProfileDesktopShortcutCreation(
      dest_path_, profile_name_, avatar);
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();

  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_SUCCESS,
            VerifyProfileShortcut(profile_name_));
  ASSERT_TRUE(file_util::PathExists(dest_path_.Append(
      FILE_PATH_LITERAL("Google Profile.ico"))));
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsUpdate) {
  if (!profile_shortcut_manager_.get())
    return;
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
            VerifyProfileShortcut(profile_name_));

  profile_manager_->profile_info_cache()->AddProfileToCache(
       dest_path_, profile_name_, string16(), 0);
  gfx::Image& avatar = ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_PROFILE_AVATAR_0);

  profile_shortcut_manager_->StartProfileDesktopShortcutCreation(
      dest_path_, profile_name_, avatar);
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_SUCCESS,
            VerifyProfileShortcut(profile_name_));
  string16 new_profile_name = ASCIIToUTF16("My New Profile Name");
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
            VerifyProfileShortcut(new_profile_name));

  // Cause an update in ProfileShortcutManager by modifying the profile info
  // cache
  profile_manager_->profile_info_cache()->SetNameOfProfileAtIndex(
      profile_manager_->profile_info_cache()->GetIndexOfProfileWithPath(
          dest_path_), new_profile_name);
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_FAILURE_UNEXPECTED,
            VerifyProfileShortcut(profile_name_));
  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_SUCCESS,
            VerifyProfileShortcut(new_profile_name));

  profile_name_ = new_profile_name;
}

