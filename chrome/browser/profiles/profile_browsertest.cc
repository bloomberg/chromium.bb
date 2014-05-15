// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "chrome/browser/profiles/startup_task_runner_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace {

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD1(OnPrefsLoaded, void(Profile*));
  MOCK_METHOD3(OnProfileCreated, void(Profile*, bool, bool));
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const base::FilePath& directory_path) {
  base::FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  std::string data("{}");
  ASSERT_TRUE(base::WriteFile(pref_path, data.c_str(), data.size()));
}

scoped_ptr<Profile> CreateProfile(
    const base::FilePath& path,
    Profile::Delegate* delegate,
    Profile::CreateMode create_mode) {
  scoped_ptr<Profile> profile(Profile::CreateProfile(
      path, delegate, create_mode));
  EXPECT_TRUE(profile.get());
  // This is necessary to avoid a memleak from BookmarkModel::Load.
  // Unfortunately, this also results in warnings during debug runs.
  StartupTaskRunnerServiceFactory::GetForProfile(profile.get())->
      StartDeferredTaskRunners();
  return profile.Pass();
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

void BlockThread(
    base::WaitableEvent* is_blocked,
    base::WaitableEvent* unblock) {
  is_blocked->Signal();
  unblock->Wait();
}

void SpinThreads() {
  // Give threads a chance to do their stuff before shutting down (i.e.
  // deleting scoped temp dir etc).
  // Should not be necessary anymore once Profile deletion is fixed
  // (see crbug.com/88586).
  content::RunAllPendingInMessageLoop();
  content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}

}  // namespace

class ProfileBrowserTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }
};

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

  {
    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), true);
  }

  SpinThreads();
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

  {
    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), false);
  }

  SpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
// This test is flaky on Linux, Win and Mac.  See crbug.com/142787
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateNewProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), true);
  }

  SpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
// Flaky: http://crbug.com/141517
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateOldProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), false);
  }

  SpinThreads();
}

// Test that a README file is created for profiles that didn't have it.
// Flaky: http://crbug.com/140882
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DISABLED_ProfileReadmeCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  // No delay before README creation.
  ProfileImpl::create_readme_delay_ms = 0;

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();

    // Wait for file thread to create the README.
    content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);

    // Verify that README exists.
    EXPECT_TRUE(base::PathExists(
        temp_dir.path().Append(chrome::kReadmeFilename)));
  }

  SpinThreads();
}

// Test that Profile can be deleted before README file is created.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ProfileDeletedBeforeReadmeCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  // No delay before README creation.
  ProfileImpl::create_readme_delay_ms = 0;

  base::WaitableEvent is_blocked(false, false);
  base::WaitableEvent* unblock = new base::WaitableEvent(false, false);

  // Block file thread.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&BlockThread, &is_blocked, base::Owned(unblock)));
  // Wait for file thread to actually be blocked.
  is_blocked.Wait();

  scoped_ptr<Profile> profile(CreateProfile(
      temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

  // Delete the Profile instance before we give the file thread a chance to
  // create the README.
  profile.reset();

  // Now unblock the file thread again and run pending tasks (this includes the
  // task for README creation).
  unblock->Signal();

  SpinThreads();
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
  {
    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

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

  SpinThreads();
}
