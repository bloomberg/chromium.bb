// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
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

void FlushTaskRunner(base::SequencedTaskRunner* runner) {
  ASSERT_TRUE(runner);
  base::WaitableEvent unblock(false, false);

  runner->PostTask(FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&unblock)));

  unblock.Wait();
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

  scoped_ptr<Profile> CreateProfile(
      const base::FilePath& path,
      Profile::Delegate* delegate,
      Profile::CreateMode create_mode) {
    scoped_ptr<Profile> profile(Profile::CreateProfile(
        path, delegate, create_mode));
    EXPECT_TRUE(profile.get());

    // Store the Profile's IO task runner so we can wind it down.
    profile_io_task_runner_ = profile->GetIOTaskRunner();

    return profile.Pass();
  }

  void FlushIoTaskRunnerAndSpinThreads() {
    FlushTaskRunner(profile_io_task_runner_.get());
    SpinThreads();
  }

  scoped_refptr<base::SequencedTaskRunner> profile_io_task_runner_;
};

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileSynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    scoped_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), true);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileSynchronous) {
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

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
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

  FlushIoTaskRunnerAndSpinThreads();
}


// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
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

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test that a README file is created for profiles that didn't have it.
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

  FlushIoTaskRunnerAndSpinThreads();
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

  FlushIoTaskRunnerAndSpinThreads();
}

// Test that repeated setting of exit type is handled correctly.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ExitType) {
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

  FlushIoTaskRunnerAndSpinThreads();
}

// The EndSession IO synchronization is only critical on Windows, but also
// happens under the USE_X11 define. See BrowserProcessImpl::EndSession.
#if defined(USE_X11) || defined(OS_WIN)

namespace {

std::string GetExitTypePreferenceFromDisk(Profile* profile) {
  base::FilePath prefs_path =
      profile->GetPath().Append(chrome::kPreferencesFilename);
  std::string prefs;
  if (!base::ReadFileToString(prefs_path, &prefs))
    return std::string();

  scoped_ptr<base::Value> value(base::JSONReader::Read(prefs));
  if (!value)
    return std::string();

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict) || !dict)
    return std::string();

  std::string exit_type;
  if (!dict->GetString("profile.exit_type", &exit_type))
    return std::string();

  return exit_type;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       WritesProfilesSynchronouslyOnEndSession) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  std::vector<Profile*> loaded_profiles = profile_manager->GetLoadedProfiles();

  ASSERT_NE(loaded_profiles.size(), 0UL);
  Profile* profile = loaded_profiles[0];

  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  bool succeeded = false;
  for (size_t retries = 0; !succeeded && retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
    profile->SetExitType(Profile::EXIT_CRASHED);
    FlushTaskRunner(profile->GetIOTaskRunner().get());

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");

    // The blocking wait in EndSession has a timeout.
    base::Time start = base::Time::Now();

    // This must not return until the profile data has been written to disk.
    // If this test flakes, then logoff on Windows has broken again.
    g_browser_process->EndSession();

    base::Time end = base::Time::Now();

    // The EndSession timeout is 10 seconds. If we take more than half that,
    // go around again, as we may have timed out on the wait.
    // This helps against flakes, and also ensures that if the IO thread starts
    // blocking systemically for that length of time (e.g. deadlocking or such),
    // we'll get a consistent test failure.
    if (end - start > base::TimeDelta::FromSeconds(5))
      continue;

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "SessionEnded");

    // Mark the success.
    succeeded = true;
  }

  ASSERT_TRUE(succeeded) << "profile->EndSession() timed out too often.";
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       EndSessionBrokenSynchronizationExperiment) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Select into the field trial group.
  base::FieldTrialList::CreateFieldTrial("WindowsLogoffRace",
                                         "BrokenSynchronization");

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  std::vector<Profile*> loaded_profiles = profile_manager->GetLoadedProfiles();

  ASSERT_NE(loaded_profiles.size(), 0UL);
  Profile* profile = loaded_profiles[0];

  bool mis_wrote = false;
  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  for (size_t retries = 0; retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
    profile->SetExitType(Profile::EXIT_CRASHED);
    FlushTaskRunner(profile->GetIOTaskRunner().get());

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");

    base::WaitableEvent is_blocked(false, false);
    base::WaitableEvent* unblock = new base::WaitableEvent(false, false);

    // Block the profile's IO thread.
    profile->GetIOTaskRunner()->PostTask(FROM_HERE,
        base::Bind(&BlockThread, &is_blocked, base::Owned(unblock)));
    // Wait for the IO thread to actually be blocked.
    is_blocked.Wait();

    // The blocking wait in EndSession has a timeout, so a non-write can only be
    // concluded if it happens in less time than the timeout.
    base::Time start = base::Time::Now();

    // With the broken synchronization this is expected to return without
    // blocking for the Profile's IO thread.
    g_browser_process->EndSession();

    base::Time end = base::Time::Now();

    // The EndSession timeout is 10 seconds, we take a 5 second run-through as
    // sufficient proof that we didn't hit the timed wait.
    if (end - start < base::TimeDelta::FromSeconds(5)) {
      // Make sure that the prefs file is unmodified with the expected
      // key/value.
      EXPECT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");
      mis_wrote = true;
    }

    // Release the IO thread thread.
    unblock->Signal();
  }

  ASSERT_TRUE(mis_wrote);
}

#endif  // defined(USE_X11) || defined(OS_WIN)
