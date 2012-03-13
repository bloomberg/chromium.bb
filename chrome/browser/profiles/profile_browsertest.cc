// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD3(OnProfileCreated, void(Profile*, bool, bool));
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const FilePath& directory_path) {
  FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  base::PlatformFile file = base::CreatePlatformFile(pref_path,
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE, NULL, NULL);
  ASSERT_TRUE(file != base::kInvalidPlatformFileValue);
  ASSERT_TRUE(base::ClosePlatformFile(file));
  std::string data("{}");
  ASSERT_TRUE(file_util::WriteFile(pref_path, data.c_str(), data.size()));
}

}  // namespace

typedef InProcessBrowserTest ProfileBrowserTest;

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileSynchronous) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileSynchronous) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());
}

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileAsynchronous) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Wait for the profile to be created.
  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(profile.get()));
  observer.Wait();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileAsynchronous) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));
  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Wait for the profile to be created.
  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(profile.get()));
  observer.Wait();
}
