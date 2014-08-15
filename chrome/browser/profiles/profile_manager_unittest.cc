// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user_manager.h"
#endif

using base::ASCIIToUTF16;
using content::BrowserThread;

namespace {

// This global variable is used to check that value returned to different
// observers is the same.
Profile* g_created_profile;

class UnittestProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(
      const base::FilePath& file_path) OVERRIDE {
    if (!base::PathExists(file_path)) {
      if (!base::CreateDirectory(file_path))
        return NULL;
    }
    return new TestingProfile(file_path, NULL);
  }

  virtual Profile* CreateProfileAsyncHelper(const base::FilePath& path,
                                            Delegate* delegate) OVERRIDE {
    // This is safe while all file operations are done on the FILE thread.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(base::IgnoreResult(&base::CreateDirectory), path));

    return new TestingProfile(path, this);
  }
};

}  // namespace

class ProfileManagerTest : public testing::Test {
 protected:
  class MockObserver {
   public:
    MOCK_METHOD2(OnProfileCreated,
        void(Profile* profile, Profile::CreateStatus status));
  };

  ProfileManagerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.path()));

#if defined(OS_CHROMEOS)
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kTestType);
#endif
  }

  virtual void TearDown() {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
  }

  // Helper function to create a profile with |name| for a profile |manager|.
  void CreateProfileAsync(ProfileManager* manager,
                          const std::string& name,
                          bool is_supervised,
                          MockObserver* mock_observer) {
    manager->CreateProfileAsync(
        temp_dir_.path().AppendASCII(name),
        base::Bind(&MockObserver::OnProfileCreated,
                   base::Unretained(mock_observer)),
        base::UTF8ToUTF16(name),
        base::UTF8ToUTF16(profiles::GetDefaultAvatarIconUrl(0)),
        is_supervised ? "Dummy ID" : std::string());
  }

  // Helper function to add a profile with |profile_name| to
  // |profile_manager|'s ProfileInfoCache, and return the profile created.
  Profile* AddProfileToCache(ProfileManager* profile_manager,
                             const std::string& path_suffix,
                             const base::string16& profile_name) {
    ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
    size_t num_profiles = cache.GetNumberOfProfiles();
    base::FilePath path = temp_dir_.path().AppendASCII(path_suffix);
    cache.AddProfileToCache(path, profile_name,
                            base::string16(), 0, std::string());
    EXPECT_EQ(num_profiles + 1, cache.GetNumberOfProfiles());
    return profile_manager->GetProfile(path);
  }

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
#endif

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  content::TestBrowserThreadBundle thread_bundle_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ProfileManagerTest, GetProfile) {
  base::FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("New Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create a profile.
  Profile* profile = profile_manager->GetProfile(dest_path);
  EXPECT_TRUE(profile);

  // The profile already exists when we call GetProfile. Just load it.
  EXPECT_EQ(profile, profile_manager->GetProfile(dest_path));
}

TEST_F(ProfileManagerTest, DefaultProfileDir) {
  base::FilePath expected_default =
      base::FilePath().AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(
      expected_default.value(),
      g_browser_process->profile_manager()->GetInitialProfileDir().value());
}

#if defined(OS_CHROMEOS)
// This functionality only exists on Chrome OS.
TEST_F(ProfileManagerTest, LoggedInProfileDir) {
  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string profile_dir(chrome::kTestUserProfileDir);

  cl->AppendSwitchASCII(chromeos::switches::kLoginProfile, profile_dir);

  base::FilePath expected_default =
      base::FilePath().AppendASCII(chrome::kInitialProfile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(expected_default.value(),
            profile_manager->GetInitialProfileDir().value());

  scoped_ptr<chromeos::MockUserManager> mock_user_manager;
  mock_user_manager.reset(new chromeos::MockUserManager());
  mock_user_manager->SetActiveUser("user@gmail.com");
  user_manager::User* active_user = mock_user_manager->GetActiveUser();
  profile_manager->Observe(
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources(),
      content::Details<const user_manager::User>(active_user));
  base::FilePath expected_logged_in(profile_dir);
  EXPECT_EQ(expected_logged_in.value(),
            profile_manager->GetInitialProfileDir().value());
  VLOG(1) << temp_dir_.path().Append(
      profile_manager->GetInitialProfileDir()).value();
}

#endif

TEST_F(ProfileManagerTest, CreateAndUseTwoProfiles) {
  base::FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.path();
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
  ASSERT_TRUE(profile1->CreateHistoryService(true, false));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(profile1,
                                                   Profile::EXPLICIT_ACCESS));
  profile1->CreateBookmarkModel(true);
  EXPECT_TRUE(BookmarkModelFactory::GetForProfile(profile1));
  profile2->CreateBookmarkModel(true);
  EXPECT_TRUE(BookmarkModelFactory::GetForProfile(profile2));
  ASSERT_TRUE(profile2->CreateHistoryService(true, false));
  EXPECT_TRUE(HistoryServiceFactory::GetForProfile(profile2,
                                                   Profile::EXPLICIT_ACCESS));

  // Make sure any pending tasks run before we destroy the profiles.
    base::RunLoop().RunUntilIdle();

  TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);

  // Make sure history cleans up correctly.
  base::RunLoop().RunUntilIdle();
}

MATCHER(NotFail, "Profile creation failure status is not reported.") {
  return arg == Profile::CREATE_STATUS_CREATED ||
         arg == Profile::CREATE_STATUS_INITIALIZED;
}

MATCHER(SameNotNull, "The same non-NULL value for all calls.") {
  if (!g_created_profile)
    g_created_profile = arg;
  return arg != NULL && arg == g_created_profile;
}

TEST_F(ProfileManagerTest, CreateProfileAsyncMultipleRequests) {
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
  const std::string profile_name = "New Profile";
  CreateProfileAsync(profile_manager, profile_name, false, &mock_observer1);
  CreateProfileAsync(profile_manager, profile_name, false, &mock_observer2);
  CreateProfileAsync(profile_manager, profile_name, false, &mock_observer3);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProfileManagerTest, CreateProfilesAsync) {
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(3));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  CreateProfileAsync(profile_manager, profile_name2, false, &mock_observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProfileManagerTest, CreateProfileAsyncCheckOmitted) {
  std::string name = "0 Supervised Profile";

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  EXPECT_EQ(0u, cache.GetNumberOfProfiles());

  CreateProfileAsync(profile_manager, name, true, &mock_observer);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, cache.GetNumberOfProfiles());
  // Supervised profiles should start out omitted from the profile list.
  EXPECT_TRUE(cache.IsOmittedProfileAtIndex(0));

  name = "1 Regular Profile";
  CreateProfileAsync(profile_manager, name, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, cache.GetNumberOfProfiles());
  // Non-supervised profiles should be included in the profile list.
  EXPECT_FALSE(cache.IsOmittedProfileAtIndex(1));
}

TEST_F(ProfileManagerTest, AddProfileToCacheCheckOmitted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  EXPECT_EQ(0u, cache.GetNumberOfProfiles());

  const base::FilePath supervised_path =
      temp_dir_.path().AppendASCII("Supervised");
  TestingProfile* supervised_profile =
      new TestingProfile(supervised_path, NULL);
  supervised_profile->GetPrefs()->SetString(prefs::kSupervisedUserId, "An ID");

  // RegisterTestingProfile adds the profile to the cache and takes ownership.
  profile_manager->RegisterTestingProfile(supervised_profile, true, false);
  EXPECT_EQ(1u, cache.GetNumberOfProfiles());
  EXPECT_TRUE(cache.IsOmittedProfileAtIndex(0));

  const base::FilePath nonsupervised_path = temp_dir_.path().AppendASCII(
      "Non-Supervised");
  TestingProfile* nonsupervised_profile = new TestingProfile(nonsupervised_path,
                                                             NULL);
  profile_manager->RegisterTestingProfile(nonsupervised_profile, true, false);

  EXPECT_EQ(2u, cache.GetNumberOfProfiles());
  size_t supervised_index = cache.GetIndexOfProfileWithPath(supervised_path);
  EXPECT_TRUE(cache.IsOmittedProfileAtIndex(supervised_index));
  size_t nonsupervised_index =
      cache.GetIndexOfProfileWithPath(nonsupervised_path);
  EXPECT_FALSE(cache.IsOmittedProfileAtIndex(nonsupervised_index));
}

TEST_F(ProfileManagerTest, GetGuestProfilePath) {
  base::FilePath guest_path = ProfileManager::GetGuestProfilePath();
  base::FilePath expected_path = temp_dir_.path();
  expected_path = expected_path.Append(chrome::kGuestProfileDir);
  EXPECT_EQ(expected_path, guest_path);
}

class UnittestGuestProfileManager : public UnittestProfileManager {
 public:
  explicit UnittestGuestProfileManager(const base::FilePath& user_data_dir)
      : UnittestProfileManager(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(
      const base::FilePath& file_path) OVERRIDE {
    TestingProfile::Builder builder;
    builder.SetGuestSession();
    builder.SetPath(file_path);
    TestingProfile* testing_profile = builder.Build().release();
    return testing_profile;
  }
};

class ProfileManagerGuestTest : public ProfileManagerTest  {
 protected:
  virtual void SetUp() {
    // Create a new temporary directory, and store the path
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestGuestProfileManager(temp_dir_.path()));

#if defined(OS_CHROMEOS)
    CommandLine* cl = CommandLine::ForCurrentProcess();
    // This switch is needed to skip non-test specific behavior in
    // ProfileManager (accessing DBusThreadManager).
    cl->AppendSwitch(switches::kTestType);

    cl->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                          std::string(chrome::kProfileDirPrefix) +
                              chromeos::login::kGuestUserName);
    cl->AppendSwitch(chromeos::switches::kGuestSession);
    cl->AppendSwitch(::switches::kIncognito);

    user_manager::UserManager::Get()->UserLoggedIn(
        chromeos::login::kGuestUserName,
        chromeos::login::kGuestUserName,
        false);
#endif
  }
};

TEST_F(ProfileManagerGuestTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_TRUE(profile->IsOffTheRecord());
}

#if defined(OS_CHROMEOS)
TEST_F(ProfileManagerGuestTest, GuestProfileIngonito) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  EXPECT_TRUE(primary_profile->IsOffTheRecord());

  Profile* active_profile = ProfileManager::GetActiveUserProfile();
  EXPECT_TRUE(active_profile->IsOffTheRecord());

  EXPECT_TRUE(active_profile->IsSameProfile(primary_profile));

  Profile* last_used_profile = ProfileManager::GetLastUsedProfile();
  EXPECT_TRUE(last_used_profile->IsOffTheRecord());

  EXPECT_TRUE(last_used_profile->IsSameProfile(active_profile));
}
#endif

TEST_F(ProfileManagerTest, AutoloadProfilesWithBackgroundApps) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  local_state_.Get()->SetUserPref(prefs::kBackgroundModeEnabled,
                                  new base::FundamentalValue(true));

  // Setting a pref which is not applicable to a system (i.e., Android in this
  // case) does not necessarily create it. Don't bother continuing with the
  // test if this pref doesn't exist because it will not load the profiles if
  // it cannot verify that the pref for background mode is enabled.
  if (!local_state_.Get()->HasPrefPath(prefs::kBackgroundModeEnabled))
    return;

  EXPECT_EQ(0u, cache.GetNumberOfProfiles());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_1"),
                          ASCIIToUTF16("name_1"), base::string16(), 0,
                          std::string());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_2"),
                          ASCIIToUTF16("name_2"), base::string16(), 0,
                          std::string());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_3"),
                          ASCIIToUTF16("name_3"), base::string16(), 0,
                          std::string());
  cache.SetBackgroundStatusOfProfileAtIndex(0, true);
  cache.SetBackgroundStatusOfProfileAtIndex(2, true);
  EXPECT_EQ(3u, cache.GetNumberOfProfiles());

  profile_manager->AutoloadProfiles();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
}

TEST_F(ProfileManagerTest, DoNotAutoloadProfilesIfBackgroundModeOff) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  local_state_.Get()->SetUserPref(prefs::kBackgroundModeEnabled,
                                  new base::FundamentalValue(false));

  EXPECT_EQ(0u, cache.GetNumberOfProfiles());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_1"),
                          ASCIIToUTF16("name_1"), base::string16(), 0,
                          std::string());
  cache.AddProfileToCache(cache.GetUserDataDir().AppendASCII("path_2"),
                          ASCIIToUTF16("name_2"), base::string16(), 0,
                          std::string());
  cache.SetBackgroundStatusOfProfileAtIndex(0, false);
  cache.SetBackgroundStatusOfProfileAtIndex(1, true);
  EXPECT_EQ(2u, cache.GetNumberOfProfiles());

  profile_manager->AutoloadProfiles();

  EXPECT_EQ(0u, profile_manager->GetLoadedProfiles().size());
}

TEST_F(ProfileManagerTest, InitProfileUserPrefs) {
  base::FilePath dest_path = temp_dir_.path();
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
  EXPECT_TRUE(profiles::IsDefaultAvatarIconIndex(
      avatar_index));
}

// Tests that a new profile's entry in the profile info cache is setup with the
// same values that are in the profile prefs.
TEST_F(ProfileManagerTest, InitProfileInfoCacheForAProfile) {
  base::FilePath dest_path = temp_dir_.path();
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
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
  EXPECT_EQ(avatar_index,
            cache.GetAvatarIconIndexOfProfileAtIndex(profile_index));
}

TEST_F(ProfileManagerTest, GetLastUsedProfileAllowedByPolicy) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  Profile* profile = profile_manager->GetLastUsedProfileAllowedByPolicy();
  ASSERT_TRUE(profile);
  EXPECT_FALSE(profile->IsOffTheRecord());
  PrefService* prefs = profile->GetPrefs();
  EXPECT_EQ(IncognitoModePrefs::ENABLED,
            IncognitoModePrefs::GetAvailability(prefs));

  ASSERT_TRUE(profile->GetOffTheRecordProfile());

  IncognitoModePrefs::SetAvailability(prefs, IncognitoModePrefs::DISABLED);
  EXPECT_FALSE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());

  // GetLastUsedProfileAllowedByPolicy() returns the incognito Profile when
  // incognito mode is forced.
  IncognitoModePrefs::SetAvailability(prefs, IncognitoModePrefs::FORCED);
  EXPECT_TRUE(
      profile_manager->GetLastUsedProfileAllowedByPolicy()->IsOffTheRecord());
}

#if !defined(OS_ANDROID)
// There's no Browser object on Android.
TEST_F(ProfileManagerTest, LastOpenedProfiles) {
  base::FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.path();
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
  Browser::CreateParams profile1_params(profile1, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser1a(
      chrome::CreateBrowserWithTestWindowForParams(&profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Adding more browsers doesn't change anything.
  scoped_ptr<Browser> browser1b(
      chrome::CreateBrowserWithTestWindowForParams(&profile1_params));
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
  base::FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));

  base::FilePath dest_path2 = temp_dir_.path();
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
  Browser::CreateParams profile1_params(profile1, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser1(
      chrome::CreateBrowserWithTestWindowForParams(&profile1_params));

  // And for profile2.
  Browser::CreateParams profile2_params(profile2, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&profile2_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);
  EXPECT_EQ(profile2, last_opened_profiles[1]);

  // Simulate a shutdown.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
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
  base::FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("New Profile 1"));
  base::FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("New Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(profile1);

  // Incognito profiles should not be managed by the profile manager but by the
  // original profile.
  TestingProfile::Builder builder;
  builder.SetIncognito();
  scoped_ptr<TestingProfile> profile2 = builder.Build();
  profile1->SetOffTheRecordProfile(profile2.PassAs<Profile>());

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(0U, last_opened_profiles.size());

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(profile1, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser1(
      chrome::CreateBrowserWithTestWindowForParams(&profile1_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // And for profile2.
  Browser::CreateParams profile2_params(profile1->GetOffTheRecordProfile(),
                                        chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser2a(
      chrome::CreateBrowserWithTestWindowForParams(&profile2_params));

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(profile1, last_opened_profiles[0]);

  // Adding more browsers doesn't change anything.
  scoped_ptr<Browser> browser2b(
      chrome::CreateBrowserWithTestWindowForParams(&profile2_params));
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

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
// There's no Browser object on Android and there's no multi-profiles on Chrome.
TEST_F(ProfileManagerTest, EphemeralProfilesDontEndUpAsLastProfile) {
  base::FilePath dest_path = temp_dir_.path();
  dest_path = dest_path.Append(FILE_PATH_LITERAL("Ephemeral Profile"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  TestingProfile* profile =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path));
  ASSERT_TRUE(profile);
  profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

  // Here the last used profile is still the "Default" profile.
  Profile* last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);

  // Create a browser for the profile.
  Browser::CreateParams profile_params(profile, chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser(
      chrome::CreateBrowserWithTestWindowForParams(&profile_params));
  last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);

  // Close the browser.
  browser.reset();
  last_used_profile = profile_manager->GetLastUsedProfile();
  EXPECT_NE(profile, last_used_profile);
}

TEST_F(ProfileManagerTest, EphemeralProfilesDontEndUpAsLastOpenedAtShutdown) {
  base::FilePath dest_path1 = temp_dir_.path();
  dest_path1 = dest_path1.Append(FILE_PATH_LITERAL("Normal Profile"));

  base::FilePath dest_path2 = temp_dir_.path();
  dest_path2 = dest_path2.Append(FILE_PATH_LITERAL("Ephemeral Profile 1"));

  base::FilePath dest_path3 = temp_dir_.path();
  dest_path3 = dest_path3.Append(FILE_PATH_LITERAL("Ephemeral Profile 2"));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // Successfully create the profiles.
  TestingProfile* normal_profile =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path1));
  ASSERT_TRUE(normal_profile);

  // Add one ephemeral profile which should not end up in this list.
  TestingProfile* ephemeral_profile1 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path2));
  ASSERT_TRUE(ephemeral_profile1);
  ephemeral_profile1->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles,
                                             true);

  // Add second ephemeral profile but don't mark it as such yet.
  TestingProfile* ephemeral_profile2 =
      static_cast<TestingProfile*>(profile_manager->GetProfile(dest_path3));
  ASSERT_TRUE(ephemeral_profile2);

  // Create a browser for profile1.
  Browser::CreateParams profile1_params(normal_profile,
                                        chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser1(
      chrome::CreateBrowserWithTestWindowForParams(&profile1_params));

  // Create browsers for the ephemeral profile.
  Browser::CreateParams profile2_params(ephemeral_profile1,
                                        chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&profile2_params));

  Browser::CreateParams profile3_params(ephemeral_profile2,
                                        chrome::GetActiveDesktop());
  scoped_ptr<Browser> browser3(
      chrome::CreateBrowserWithTestWindowForParams(&profile3_params));

  std::vector<Profile*> last_opened_profiles =
      profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(2U, last_opened_profiles.size());
  EXPECT_EQ(normal_profile, last_opened_profiles[0]);
  EXPECT_EQ(ephemeral_profile2, last_opened_profiles[1]);

  // Mark the second profile ephemeral.
  ephemeral_profile2->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles,
                                             true);

  // Simulate a shutdown.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CLOSE_ALL_BROWSERS_REQUEST,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
  browser1.reset();
  browser2.reset();
  browser3.reset();

  last_opened_profiles = profile_manager->GetLastOpenedProfiles();
  ASSERT_EQ(1U, last_opened_profiles.size());
  EXPECT_EQ(normal_profile, last_opened_profiles[0]);
}

TEST_F(ProfileManagerTest, ActiveProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load two profiles.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  base::FilePath dest_path1 = temp_dir_.path().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.path().AppendASCII(profile_name2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(3));

  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  CreateProfileAsync(profile_manager, profile_name2, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(2u, profile_manager->GetProfileInfoCache().GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  // Delete the active profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1,
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
}

TEST_F(ProfileManagerTest, LastProfileDeleted) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load a profile.
  const std::string profile_name1 = "New Profile 1";
  base::FilePath dest_path1 = temp_dir_.path().AppendASCII(profile_name1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(1));

  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, profile_manager->GetProfileInfoCache().GetNumberOfProfiles());

  // Set it as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, profile_name1);

  // Delete the active profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1,
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();

  // A new profile should have been created
  const std::string profile_name2 = "Profile 1";
  base::FilePath dest_path2 = temp_dir_.path().AppendASCII(profile_name2);

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
  EXPECT_EQ(dest_path2,
      profile_manager->GetProfileInfoCache().GetPathOfProfileAtIndex(0));
}

TEST_F(ProfileManagerTest, LastProfileDeletedWithGuestActiveProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load a profile.
  const std::string profile_name1 = "New Profile 1";
  base::FilePath dest_path1 = temp_dir_.path().AppendASCII(profile_name1);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));

  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, profile_manager->GetProfileInfoCache().GetNumberOfProfiles());

  // Create the profile and register it.
  const std::string guest_profile_name =
      ProfileManager::GetGuestProfilePath().BaseName().MaybeAsASCII();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());
  TestingProfile* guest_profile = builder.Build().release();
  guest_profile->set_profile_name(guest_profile_name);
  // Registering the profile passes ownership to the ProfileManager.
  profile_manager->RegisterTestingProfile(guest_profile, false, false);

  // The Guest profile does not get added to the ProfileInfoCache.
  EXPECT_EQ(2u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, profile_manager->GetProfileInfoCache().GetNumberOfProfiles());

  // Set the Guest profile as the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed, guest_profile_name);

  // Delete the other profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1,
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();

  // A new profile should have been created.
  const std::string profile_name2 = "Profile 1";
  base::FilePath dest_path2 = temp_dir_.path().AppendASCII(profile_name2);

  EXPECT_EQ(3u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(1u, profile_manager->GetProfileInfoCache().GetNumberOfProfiles());
  EXPECT_EQ(dest_path2,
      profile_manager->GetProfileInfoCache().GetPathOfProfileAtIndex(0));
}

TEST_F(ProfileManagerTest, ProfileDisplayNameResetsDefaultName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // The command line is reset at the end of every test by the test suite.
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  EXPECT_EQ(0u, cache.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = cache.ChooseNameForNewProfile(0);
  Profile* profile1 = AddProfileToCache(profile_manager,
                                        "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = cache.ChooseNameForNewProfile(1);
  Profile* profile2 = AddProfileToCache(profile_manager,
                                        "path_2", profile_name2);
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the default name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}

TEST_F(ProfileManagerTest, ProfileDisplayNamePreservesCustomName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // The command line is reset at the end of every test by the test suite.
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  EXPECT_EQ(0u, cache.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = cache.ChooseNameForNewProfile(0);
  Profile* profile1 = AddProfileToCache(profile_manager,
                                        "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // We should display custom names for local profiles.
  const base::string16 custom_profile_name = ASCIIToUTF16("Batman");
  cache.SetNameOfProfileAtIndex(0, custom_profile_name);
  EXPECT_EQ(custom_profile_name, cache.GetNameOfProfileAtIndex(0));
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = cache.ChooseNameForNewProfile(1);
  Profile* profile2 = AddProfileToCache(profile_manager,
                                        "path_2", profile_name2);
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, custom name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(custom_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}

TEST_F(ProfileManagerTest, ProfileDisplayNamePreservesSignedInName) {
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  // The command line is reset at the end of every test by the test suite.
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  EXPECT_EQ(0u, cache.GetNumberOfProfiles());

  // Only one local profile means we display IDS_SINGLE_PROFILE_DISPLAY_NAME.
  const base::string16 default_profile_name =
      l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
  const base::string16 profile_name1 = cache.ChooseNameForNewProfile(0);
  Profile* profile1 = AddProfileToCache(profile_manager,
                                        "path_1", profile_name1);
  EXPECT_EQ(default_profile_name,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // We should display the actual profile name for signed in profiles.
  cache.SetUserNameOfProfileAtIndex(0, ASCIIToUTF16("user@gmail.com"));
  EXPECT_EQ(profile_name1, cache.GetNameOfProfileAtIndex(0));
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));

  // Multiple profiles means displaying the actual profile names.
  const base::string16 profile_name2 = cache.ChooseNameForNewProfile(1);
  Profile* profile2 = AddProfileToCache(profile_manager,
                                        "path_2", profile_name2);
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
  EXPECT_EQ(profile_name2,
            profiles::GetAvatarNameForProfile(profile2->GetPath()));

  // Deleting a profile means returning to the original, actual profile name.
  profile_manager->ScheduleProfileForDeletion(profile2->GetPath(),
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(profile_name1,
            profiles::GetAvatarNameForProfile(profile1->GetPath()));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// These tests are for a Mac-only code path that assumes the browser
// process isn't killed when all browser windows are closed.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNeedsToLoadNextProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and just create a second profile.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  base::FilePath dest_path1 = temp_dir_.path().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.path().AppendASCII(profile_name2);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));
  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  // Track the profile, but don't load it.
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  cache.AddProfileToCache(dest_path2, ASCIIToUTF16(profile_name2),
                          base::string16(), 0, std::string());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(2u, cache.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  // Delete the active profile. This should switch and load the unloaded
  // profile.
  profile_manager->ScheduleProfileForDeletion(dest_path1,
                                              ProfileManager::CreateCallback());

  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path2, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name2, local_state->GetString(prefs::kProfileLastUsed));
}

// This tests the recursive call in ProfileManager::OnNewActiveProfileLoaded
// by simulating a scenario in which the profile that is being loaded as
// the next active profile has also been marked for deletion, so the
// ProfileManager needs to recursively select a different next profile.
TEST_F(ProfileManagerTest, ActiveProfileDeletedNextProfileDeletedToo) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);

  // Create and load one profile, and create two more profiles.
  const std::string profile_name1 = "New Profile 1";
  const std::string profile_name2 = "New Profile 2";
  const std::string profile_name3 = "New Profile 3";
  base::FilePath dest_path1 = temp_dir_.path().AppendASCII(profile_name1);
  base::FilePath dest_path2 = temp_dir_.path().AppendASCII(profile_name2);
  base::FilePath dest_path3 = temp_dir_.path().AppendASCII(profile_name3);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer, OnProfileCreated(
      testing::NotNull(), NotFail())).Times(testing::AtLeast(2));
  CreateProfileAsync(profile_manager, profile_name1, false, &mock_observer);
  base::RunLoop().RunUntilIdle();

  // Create the other profiles, but don't load them. Assign a fake avatar icon
  // to ensure that profiles in the info cache are sorted by the profile name,
  // and not randomly by the avatar name.
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  cache.AddProfileToCache(dest_path2, ASCIIToUTF16(profile_name2),
                          ASCIIToUTF16(profile_name2), 1, std::string());
  cache.AddProfileToCache(dest_path3, ASCIIToUTF16(profile_name3),
                          ASCIIToUTF16(profile_name3), 2, std::string());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, profile_manager->GetLoadedProfiles().size());
  EXPECT_EQ(3u, cache.GetNumberOfProfiles());

  // Set the active profile.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path1.BaseName().MaybeAsASCII());

  // Delete the active profile, Profile1.
  // This will post a CreateProfileAsync message, that tries to load Profile2,
  // which checks that the profile is not being deleted, and then calls back
  // FinishDeletingProfile for Profile1.
  // Try to break this flow by setting the active profile to Profile2 in the
  // middle (so after the first posted message), and trying to delete Profile2,
  // so that the ProfileManager has to look for a different profile to load.
  profile_manager->ScheduleProfileForDeletion(dest_path1,
                                              ProfileManager::CreateCallback());
  local_state->SetString(prefs::kProfileLastUsed,
                         dest_path2.BaseName().MaybeAsASCII());
  profile_manager->ScheduleProfileForDeletion(dest_path2,
                                              ProfileManager::CreateCallback());
  // Spin the message loop so that all the callbacks can finish running.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dest_path3, profile_manager->GetLastUsedProfile()->GetPath());
  EXPECT_EQ(profile_name3, local_state->GetString(prefs::kProfileLastUsed));
}
#endif  // !defined(OS_MACOSX)
