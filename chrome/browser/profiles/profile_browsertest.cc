// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/platform_file.h"
#include "base/version.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
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

void CheckChromeVersion(Profile *profile, bool is_new) {
  std::string created_by_version;
  if (is_new) {
    chrome::VersionInfo version_info;
    created_by_version = version_info.Version();
  } else {
    created_by_version = "1.0.0.0";
  }
  std::string pref_version =
      ChromeVersionService::GetVersion(profile->GetPrefs());
  // Assert that created_by_version pref gets set to current version.
  EXPECT_EQ(created_by_version, pref_version);
}

}  // namespace

typedef InProcessBrowserTest ProfileBrowserTest;

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
//
// Flaky (sometimes timeout): http://crbug.com/141141
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateNewProfileSynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());
  CheckChromeVersion(profile.get(), true);
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
// Flaky: http://crbug.com/141517
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateOldProfileSynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());
  CheckChromeVersion(profile.get(), false);
}

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       CreateNewProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Wait for the profile to be created.
  observer.Wait();
  CheckChromeVersion(profile.get(), true);
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       CreateOldProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Wait for the profile to be created.
  observer.Wait();
  CheckChromeVersion(profile.get(), false);
}

// Vista bots were timing out often. http://crbug.com/140882
#if defined(OS_WIN)
#define MAYBE_ProfileReadmeCreated DISABLED_ProfileReadmeCreated
#else
#define MAYBE_ProfileReadmeCreated ProfileReadmeCreated
#endif
// Test that a README file is created for profiles that didn't have it.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_ProfileReadmeCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());

  // No delay before README creation.
  ProfileImpl::create_readme_delay_ms = 0;

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Wait for the profile to be created.
  observer.Wait();

  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);

  // Verify that README exists.
  EXPECT_TRUE(file_util::PathExists(
      temp_dir.path().Append(chrome::kReadmeFilename)));
}

// Test that Profile can be deleted before README file is created.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ProfileDeletedBeforeReadmeCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  // No delay before README creation.
  ProfileImpl::create_readme_delay_ms = 0;

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  // Delete the Profile instance and run pending tasks (this includes the task
  // for README creation).
  profile.reset();
  content::RunAllPendingInMessageLoop();
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}

// Test that repeated setting of exit type is handled correctly.
#if defined(OS_WIN)
// Flaky on Windows: http://crbug.com/163713
#define MAYBE_ExitType DISABLED_ExitType
#else
#define MAYBE_ExitType ExitType
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_ExitType) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  scoped_ptr<Profile> profile(Profile::CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
  ASSERT_TRUE(profile.get());

  PrefService* prefs = profile->GetPrefs();
  // The initial state is crashed; store for later reference.
  std::string crash_value(prefs->GetString(prefs::kSessionExitType));

  // The first call to a type other than crashed should change the value.
  profile->SetExitType(Profile::EXIT_SESSION_ENDED);
  std::string first_call_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_NE(crash_value, first_call_value);

  // Subsequent calls to a non-crash value should be ignored.
  profile->SetExitType(Profile::EXIT_NORMAL);
  std::string second_call_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_EQ(first_call_value, second_call_value);

  // Setting back to a crashed value should work.
  profile->SetExitType(Profile::EXIT_CRASHED);
  std::string final_value(prefs->GetString(prefs::kSessionExitType));
  EXPECT_EQ(crash_value, final_value);
}
