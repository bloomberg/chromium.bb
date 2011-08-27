// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// This global variable is used to check that value returned to different
// observers is the same.
Profile* g_created_profile;

}  // namespace

class ProfileManagerTest : public testing::Test {
 protected:
  ProfileManagerTest()
      : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
        extension_event_router_forwarder_(new ExtensionEventRouterForwarder),
        ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(local_state_.Get(), NULL, extension_event_router_forwarder_),
        profile_manager_(new ProfileManagerWithoutInit) {
#if defined(OS_MACOSX)
    base::SystemMonitor::AllocateSystemIOPorts();
#endif
    system_monitor_dummy_.reset(new base::SystemMonitor);
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetIOThread(
        &io_thread_);
  }

  virtual void SetUp() {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() {
    profile_manager_.reset();
    message_loop_.RunAllPending();
  }

  class MockObserver : public ProfileManagerObserver {
   public:
    MOCK_METHOD2(OnProfileCreated, void(Profile* profile, Status status));
  };

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;
  scoped_refptr<ExtensionEventRouterForwarder>
      extension_event_router_forwarder_;

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  BrowserThread file_thread_;
  IOThread io_thread_;

  scoped_ptr<base::SystemMonitor> system_monitor_dummy_;

  // Also will test profile deletion.
  scoped_ptr<ProfileManager> profile_manager_;
};

TEST_F(ProfileManagerTest, GetProfile) {
  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  Profile* profile;

  // Successfully create a profile.
  profile = profile_manager_->GetProfile(dest_path);
  EXPECT_TRUE(profile);

  // The profile already exists when we call GetProfile. Just load it.
  EXPECT_EQ(profile, profile_manager_->GetProfile(dest_path));
}

TEST_F(ProfileManagerTest, DefaultProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string profile_dir("my_user");

  cl->AppendSwitch(switches::kTestType);

  FilePath expected_default =
      FilePath().AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(expected_default.value(),
            profile_manager_->GetInitialProfileDir().value());
}

#if defined(OS_CHROMEOS)
// This functionality only exists on Chrome OS.
TEST_F(ProfileManagerTest, LoggedInProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string profile_dir("my_user");

  cl->AppendSwitchASCII(switches::kLoginProfile, profile_dir);
  cl->AppendSwitch(switches::kTestType);

  FilePath expected_default =
      FilePath().AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(expected_default.value(),
            profile_manager_->GetInitialProfileDir().value());

  profile_manager_->Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                           NotificationService::AllSources(),
                           NotificationService::NoDetails());
  FilePath expected_logged_in(profile_dir);
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager_->GetInitialProfileDir().value());
  VLOG(1) << temp_dir_.path().Append(
      profile_manager_->GetInitialProfileDir()).value();
}

#endif

TEST_F(ProfileManagerTest, RegisterProfileName) {
  // Create a test profile.
  FilePath dest_path1 = temp_dir_.path();
  std::string dir_base("New Profile 1");
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));
  Profile* profile1;
  profile1 = profile_manager_->GetProfile(dest_path1);
  ASSERT_TRUE(profile1);

  // Simulate registration of the profile name (normally triggered by the
  // change of the kGoogleServicesUsername preference):
  std::string testname("testname");
  profile1->GetPrefs()->SetString(prefs::kGoogleServicesUsername, testname);
  profile_manager_->RegisterProfileName(profile1);

  // Profile name should be associated with the directory in Local State.
  std::string stored_profile_name;
  const DictionaryValue* path_map = static_cast<const DictionaryValue*>(
      local_state_.Get()->GetUserPref(prefs::kProfileDirectoryMap));
  path_map->GetString(dir_base, &stored_profile_name);
  ASSERT_STREQ(testname.c_str(), stored_profile_name.c_str());
}

TEST_F(ProfileManagerTest, CreateAndUseTwoProfiles) {
  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  Profile* profile1;
  Profile* profile2;

  // Successfully create the profiles.
  profile1 = profile_manager_->GetProfile(dest_path1);
  ASSERT_TRUE(profile1);

  profile2 = profile_manager_->GetProfile(dest_path2);
  ASSERT_TRUE(profile2);

  // Force lazy-init of some profile services to simulate use.
  EXPECT_TRUE(profile1->GetHistoryService(Profile::EXPLICIT_ACCESS));
  EXPECT_TRUE(profile1->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetBookmarkModel());
  EXPECT_TRUE(profile2->GetHistoryService(Profile::EXPLICIT_ACCESS));

  // Make sure any pending tasks run before we destroy the profiles.
  message_loop_.RunAllPending();

  profile_manager_.reset();

  // Make sure history cleans up correctly.
  message_loop_.RunAllPending();
}

MATCHER(NotFail, "Profile creation failure status is not reported.") {
  return arg == ProfileManagerObserver::STATUS_CREATED ||
      arg == ProfileManagerObserver::STATUS_INITIALIZED;
}

// Tests asynchronous profile creation mechanism.
TEST_F(ProfileManagerTest, DISABLED_CreateProfileAsync) {
  FilePath dest_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("New Profile"));

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(1));

  profile_manager_->CreateProfileAsync(dest_path, &mock_observer);

  message_loop_.RunAllPending();
}

MATCHER(SameNotNull, "The same non-NULL value for all calls.") {
  if (!g_created_profile)
    g_created_profile = arg;
  return arg != NULL && arg == g_created_profile;
}

TEST_F(ProfileManagerTest, CreateProfileAsyncMultipleRequests) {
  FilePath dest_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("New Profile"));

  g_created_profile = NULL;

  MockObserver mock_observer1;
  EXPECT_CALL(mock_observer1, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));
  MockObserver mock_observer2;
  EXPECT_CALL(mock_observer2, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));
  MockObserver mock_observer3;
  EXPECT_CALL(mock_observer3, OnProfileCreated(
      SameNotNull(), NotFail())).Times(testing::AtLeast(1));

  profile_manager_->CreateProfileAsync(dest_path, &mock_observer1);
  profile_manager_->CreateProfileAsync(dest_path, &mock_observer2);
  profile_manager_->CreateProfileAsync(dest_path, &mock_observer3);

  message_loop_.RunAllPending();
}

TEST_F(ProfileManagerTest, CreateProfilesAsync) {
  FilePath dest_path1 =
      temp_dir_.path().Append(FILE_PATH_LITERAL("New Profile 1"));
  FilePath dest_path2 =
      temp_dir_.path().Append(FILE_PATH_LITERAL("New Profile 2"));

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(3));

  profile_manager_->CreateProfileAsync(dest_path1, &mock_observer);
  profile_manager_->CreateProfileAsync(dest_path2, &mock_observer);

  message_loop_.RunAllPending();
}
