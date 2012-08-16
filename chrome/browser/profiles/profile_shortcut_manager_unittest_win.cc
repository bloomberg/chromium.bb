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
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

class ProfileShortcutManagerTest : public testing::Test {
 protected:
  ProfileShortcutManagerTest()
      : file_thread_(BrowserThread::FILE, &message_loop_) {
  }

  virtual void SetUp() {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
  }

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread file_thread_;
};

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsIconExists) {
  // Profile shortcut manager will be NULL for non-windows platforms
  ProfileShortcutManager* profile_shortcut_manager =
      ProfileShortcutManager::Create();

  if (!profile_shortcut_manager)
    return;

  FilePath dest_path = temp_dir_.path();
  string16 profile_name = ASCIIToUTF16("My Profile");

  gfx::Image& avatar = ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_PROFILE_AVATAR_0);

  profile_shortcut_manager->CreateChromeDesktopShortcut(dest_path,
                                                        profile_name, avatar);

  ASSERT_TRUE(file_util::PathExists(dest_path.Append(
      (FILE_PATH_LITERAL("Google Profile.ico")))));

  profile_shortcut_manager->DeleteChromeDesktopShortcut(dest_path);

  // TODO(hallielaine): Verify shortcut deletion
}

TEST_F(ProfileShortcutManagerTest, DesktopShortcutsLnk) {
  // Profile shortcut manager will be NULL for non-windows platforms
  ProfileShortcutManager* profile_shortcut_manager =
      ProfileShortcutManager::Create();

  if (!profile_shortcut_manager)
    return;

  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile 1"));

  gfx::Image& avatar = ResourceBundle::GetSharedInstance().
      GetNativeImageNamed(IDR_PROFILE_AVATAR_0);

  profile_shortcut_manager->CreateChromeDesktopShortcut(dest_path,
      ASCIIToUTF16("My Profile"), avatar);

  FilePath exe_path;
  ASSERT_TRUE(PathService::Get(base::FILE_EXE, &exe_path));

  FilePath shortcut;
  string16 shortcut_name;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Get the desktop path of the current user
  ShellUtil::GetDesktopPath(false, &shortcut);
  // Get the name of the shortcut with profile attached
  ShellUtil::GetChromeShortcutName(dist, false, ASCIIToUTF16("My Profile"),
      &shortcut_name);
  shortcut = shortcut.Append(shortcut_name);

  EXPECT_EQ(ShellUtil::VERIFY_SHORTCUT_SUCCESS,
      ShellUtil::VerifyChromeShortcut(exe_path.value(),
          shortcut.value(), dist->GetAppDescription(), 0));

  profile_shortcut_manager->DeleteChromeDesktopShortcut(dest_path);
}
