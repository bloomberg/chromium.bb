// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#endif

using content::BrowserThread;

namespace {

// This global variable is used to check that value returned to different
// observers is the same.
Profile* g_created_profile;

}  // namespace

namespace testing {

class ProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit ProfileManager(const FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(const FilePath& file_path) OVERRIDE {
    if (!file_util::PathExists(file_path)) {
      if (!file_util::CreateDirectory(file_path))
        return NULL;
    }
    return new TestingProfile(file_path, NULL);
  }

  virtual Profile* CreateProfileAsyncHelper(const FilePath& path,
                                            Delegate* delegate) OVERRIDE {
    // This is safe while all file operations are done on the FILE thread.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(base::IgnoreResult(&file_util::CreateDirectory), path));

    return new TestingProfile(path, this);
  }

#if defined(OS_WIN)
  virtual ProfileShortcutManagerWin* CreateShortcutManager() OVERRIDE {
    // We should avoid creating shortcuts in these tests.
    return NULL;
  }
#endif
};

}  // namespace testing

class ProfileManagerTest : public testing::Test {
 protected:
  class MockObserver {
   public:
    MOCK_METHOD2(OnProfileCreated,
        void(Profile* profile, Profile::CreateStatus status));
  };

  ProfileManagerTest()
      : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
        extension_event_router_forwarder_(new ExtensionEventRouterForwarder),
        ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(local_state_.Get(), NULL,
                   extension_event_router_forwarder_) {
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
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetProfileManager(
        new testing::ProfileManager(temp_dir_.path()));
#if defined(OS_CHROMEOS)
  CommandLine *cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kTestType);
#endif
  }

  virtual void TearDown() {
    static_cast<TestingBrowserProcess*>(g_browser_process)->SetProfileManager(
        NULL);
    message_loop_.RunAllPending();
  }

#if defined(OS_CHROMEOS)
  // Do not change order of stub_cros_enabler_, which needs to be constructed
  // before io_thread_ which requires CrosLibrary to be initialized to construct
  // its data member pref_proxy_config_tracker_ on ChromeOS.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
#endif

  // The path to temporary directory used to contain the test operations.
  ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;
  scoped_refptr<ExtensionEventRouterForwarder>
      extension_event_router_forwarder_;

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread file_thread_;
  // IOThread is necessary for the creation of some services below.
  IOThread io_thread_;

  scoped_ptr<base::SystemMonitor> system_monitor_dummy_;
};

TEST_F(ProfileManagerTest, GetProfile) {
  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create a profile.
  Profile* profile = profile_manager->GetProfile(dest_path);
  EXPECT_TRUE(profile);

  // The profile already exists when we call GetProfile. Just load it.
  EXPECT_EQ(profile, profile_manager->GetProfile(dest_path));
}

TEST_F(ProfileManagerTest, DefaultProfileDir) {
  FilePath expected_default =
      FilePath().AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(
      expected_default.value(),
      g_browser_process->profile_manager()->GetInitialProfileDir().value());
}

#if defined(OS_CHROMEOS)
// This functionality only exists on Chrome OS.
TEST_F(ProfileManagerTest, LoggedInProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string profile_dir("my_user");

  cl->AppendSwitchASCII(switches::kLoginProfile, profile_dir);

  FilePath expected_default =
      FilePath().AppendASCII(chrome::kInitialProfile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(expected_default.value(),
            profile_manager->GetInitialProfileDir().value());

  profile_manager->Observe(chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                           content::NotificationService::AllSources(),
                           content::NotificationService::NoDetails());
  FilePath expected_logged_in(profile_dir);
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager->GetInitialProfileDir().value());
  VLOG(1) << temp_dir_.path().Append(
      profile_manager->GetInitialProfileDir()).value();
}

#endif

TEST_F(ProfileManagerTest, CreateAndUseTwoProfiles) {
  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  // Force lazy-init of some profile services to simulate use.
  profile1->CreateHistoryService(true, false);
  EXPECT_TRUE(profile1->GetHistoryService(Profile::EXPLICIT_ACCESS));
  profile1->CreateBookmarkModel(true);
  EXPECT_TRUE(profile1->GetBookmarkModel());
  profile2->CreateBookmarkModel(true);
  EXPECT_TRUE(profile2->GetBookmarkModel());
  profile2->CreateHistoryService(true, false);
  EXPECT_TRUE(profile2->GetHistoryService(Profile::EXPLICIT_ACCESS));

  // Make sure any pending tasks run before we destroy the profiles.
  message_loop_.RunAllPending();

  static_cast<TestingBrowserProcess*>(g_browser_process)->SetProfileManager(
      NULL);

  // Make sure history cleans up correctly.
  message_loop_.RunAllPending();
}

MATCHER(NotFail, "Profile creation failure status is not reported.") {
  return arg == Profile::CREATE_STATUS_CREATED ||
         arg == Profile::CREATE_STATUS_INITIALIZED;
}

// Tests asynchronous profile creation mechanism.
TEST_F(ProfileManagerTest, DISABLED_CreateProfileAsync) {
  FilePath dest_path =
      temp_dir_.path().Append(FILE_PATH_LITERAL("New Profile"));

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(1));

  g_browser_process->profile_manager()->CreateProfileAsync(dest_path,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer)));

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

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  profile_manager->CreateProfileAsync(dest_path,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer1)));
  profile_manager->CreateProfileAsync(dest_path,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer2)));
  profile_manager->CreateProfileAsync(dest_path,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer3)));

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

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  profile_manager->CreateProfileAsync(dest_path1,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer)));
  profile_manager->CreateProfileAsync(dest_path2,
      base::Bind(&MockObserver::OnProfileCreated,
                 base::Unretained(&mock_observer)));

  message_loop_.RunAllPending();
}

TEST_F(ProfileManagerTest, AutoloadProfilesWithBackgroundApps) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  EXPECT_EQ(0u, cache.GetNumberOfProfiles());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_1"),
                          ASCIIToUTF16("name_1"), string16(), 0);
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_2"),
                          ASCIIToUTF16("name_2"), string16(), 0);
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_3"),
                          ASCIIToUTF16("name_3"), string16(), 0);
  cache.SetBackgroundStatusOfProfileAtIndex(0, true);
  cache.SetBackgroundStatusOfProfileAtIndex(2, true);
  EXPECT_EQ(3u, cache.GetNumberOfProfiles());

  profile_manager->AutoloadProfiles();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
}

TEST_F(ProfileManagerTest, InitProfileUserPrefs) {
  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile;

  // Successfully create the profile
  profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(profile);

  // Check that the profile name is non empty
  std::string profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  EXPECT_FALSE(profile_name.empty());

  // Check that the profile avatar index is valid
  size_t avatar_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);
  EXPECT_TRUE(profile_manager->GetProfileInfoCache().IsDefaultAvatarIconIndex(
      avatar_index));
}

// Tests that a new profile's entry in the profile info cache is setup with the
// same values that are in the profile prefs.
TEST_F(ProfileManagerTest, InitProfileInfoCacheForAProfile) {
  FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  // Successfully create the profile
  Profile* profile = profile_manager->GetProfile(dest_path);
  ASSERT_TRUE(profile);

  std::string profile_name =
      profile->GetPrefs()->GetString(prefs::kProfileName);
  size_t avatar_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  size_t profile_index = cache.GetIndexOfProfileWithPath(dest_path);

  // Check if the profile prefs are the same as the cache prefs
  EXPECT_EQ(profile_name,
            UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
  EXPECT_EQ(avatar_index,
            cache.GetAvatarIconIndexOfProfileAtIndex(profile_index));
}

#if !defined(OS_ANDROID)
// There's no Browser object on Android.
TEST_F(ProfileManagerTest, LastOpenedProfiles) {
  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());

  // Create a browser for profile1.
  scoped_ptr<Browser> browser1a(new Browser(Browser::TYPE_TABBED, profile1));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  scoped_ptr<Browser> browser2(new Browser(Browser::TYPE_TABBED, profile2));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Adding more browsers doesn't change anything.
  scoped_ptr<Browser> browser1b(new Browser(Browser::TYPE_TABBED, profile1));
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Close the browsers.
  browser1a.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  browser1b.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile2, last_opened_profiles[0]);

  browser2.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());
}

TEST_F(ProfileManagerTest, LastOpenedProfilesAtShutdown) {
  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);

  // Create a browser for profile1.
  scoped_ptr<Browser> browser1(new Browser(Browser::TYPE_TABBED, profile1));

  // And for profile2.
  scoped_ptr<Browser> browser2(new Browser(Browser::TYPE_TABBED, profile2));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Simulate a shutdown.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_APP_EXITING,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  // Even if the browsers are destructed during shutdown, the profiles stay
  // open.
  browser1.reset();
  browser2.reset();

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);
}

TEST_F(ProfileManagerTest, LastOpenedProfilesDoesNotContainIncognito) {
  FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));
  FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  Profile* profile1 = profile_manager->GetProfile(dest_path1);
  ASSERT_TRUE(profile1);

  TestingProfile* profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(profile2);
  profile2->set_incognito(true);

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());

  // Create a browser for profile1.
  scoped_ptr<Browser> browser1(new Browser(Browser::TYPE_TABBED, profile1));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  scoped_ptr<Browser> browser2a(new Browser(Browser::TYPE_TABBED, profile2));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Adding more browsers doesn't change anything.
  scoped_ptr<Browser> browser2b(new Browser(Browser::TYPE_TABBED, profile1));
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Close the browsers.
  browser2a.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  browser2b.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  browser1.reset();
  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());
}
#endif  // !defined(OS_ANDROID)
