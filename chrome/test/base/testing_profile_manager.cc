// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

const std::string kGuestProfileName = "Guest";

namespace testing {

class ProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit ProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(
      const base::FilePath& file_path) OVERRIDE {
    return new TestingProfile(file_path);
  }
};

}  // namespace testing

TestingProfileManager::TestingProfileManager(TestingBrowserProcess* process)
    : called_set_up_(false),
      browser_process_(process),
      local_state_(process),
      profile_manager_(NULL) {
}

TestingProfileManager::~TestingProfileManager() {
  // Destroying this class also destroys the LocalState, so make sure the
  // associated ProfileManager is also destroyed.
  browser_process_->SetProfileManager(NULL);
}

bool TestingProfileManager::SetUp() {
  SetUpInternal();
  return called_set_up_;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& profile_name,
    scoped_ptr<PrefServiceSyncable> prefs,
    const base::string16& user_name,
    int avatar_id,
    const std::string& supervised_user_id,
    const TestingProfile::TestingFactories& factories) {
  DCHECK(called_set_up_);

  // Create a path for the profile based on the name.
  base::FilePath profile_path(profiles_dir_.path());
  profile_path = profile_path.AppendASCII(profile_name);

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetPath(profile_path);
  builder.SetPrefService(prefs.Pass());
  builder.SetSupervisedUserId(supervised_user_id);

  for (TestingProfile::TestingFactories::const_iterator it = factories.begin();
       it != factories.end(); ++it) {
    builder.AddTestingFactory(it->first, it->second);
  }

  TestingProfile* profile = builder.Build().release();
  profile->set_profile_name(profile_name);
  profile_manager_->AddProfile(profile);  // Takes ownership.

  // Update the user metadata.
  ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  cache.SetAvatarIconOfProfileAtIndex(index, avatar_id);
  cache.SetSupervisedUserIdOfProfileAtIndex(index, supervised_user_id);
  // SetNameOfProfileAtIndex may reshuffle the list of profiles, so we do it
  // last.
  cache.SetNameOfProfileAtIndex(index, user_name);

  testing_profiles_.insert(std::make_pair(profile_name, profile));

  return profile;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(name, scoped_ptr<PrefServiceSyncable>(),
                              base::UTF8ToUTF16(name), 0, std::string(),
                              TestingProfile::TestingFactories());
}

TestingProfile* TestingProfileManager::CreateGuestProfile() {
  DCHECK(called_set_up_);

  // Set up a profile with an off the record profile.
  TestingProfile::Builder otr_builder;
  otr_builder.SetIncognito();
  scoped_ptr<TestingProfile> otr_profile(otr_builder.Build());

  // Create the profile and register it.
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.SetPath(ProfileManager::GetGuestProfilePath());

  // Add the guest profile to the profile manager, but not to the info cache.
  TestingProfile* profile = builder.Build().release();
  profile->set_profile_name(kGuestProfileName);

  otr_profile->SetOriginalProfile(profile);
  profile->SetOffTheRecordProfile(otr_profile.PassAs<Profile>());
  profile_manager_->AddProfile(profile);  // Takes ownership.
  profile_manager_->SetGuestProfilePrefs(profile);

  testing_profiles_.insert(std::make_pair(kGuestProfileName, profile));

  return profile;
}

void TestingProfileManager::DeleteTestingProfile(const std::string& name) {
  DCHECK(called_set_up_);

  TestingProfilesMap::iterator it = testing_profiles_.find(name);
  DCHECK(it != testing_profiles_.end());

  TestingProfile* profile = it->second;

  ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  cache.DeleteProfileFromCache(profile->GetPath());

  profile_manager_->profiles_info_.erase(profile->GetPath());
}

void TestingProfileManager::DeleteAllTestingProfiles() {
  for (TestingProfilesMap::iterator it = testing_profiles_.begin();
       it != testing_profiles_.end(); ++it) {
    TestingProfile* profile = it->second;
    ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
    cache.DeleteProfileFromCache(profile->GetPath());
  }
  testing_profiles_.clear();
}


void TestingProfileManager::DeleteGuestProfile() {
  DCHECK(called_set_up_);

  TestingProfilesMap::iterator it = testing_profiles_.find(kGuestProfileName);
  DCHECK(it != testing_profiles_.end());

  profile_manager_->profiles_info_.erase(ProfileManager::GetGuestProfilePath());
}

void TestingProfileManager::DeleteProfileInfoCache() {
  profile_manager_->profile_info_cache_.reset(NULL);
}

void TestingProfileManager::SetLoggedIn(bool logged_in) {
  profile_manager_->logged_in_ = logged_in;
}

const base::FilePath& TestingProfileManager::profiles_dir() {
  DCHECK(called_set_up_);
  return profiles_dir_.path();
}

ProfileManager* TestingProfileManager::profile_manager() {
  DCHECK(called_set_up_);
  return profile_manager_;
}

ProfileInfoCache* TestingProfileManager::profile_info_cache() {
  DCHECK(called_set_up_);
  return &profile_manager_->GetProfileInfoCache();
}

void TestingProfileManager::SetUpInternal() {
  ASSERT_FALSE(browser_process_->profile_manager())
      << "ProfileManager already exists";

  // Set up the directory for profiles.
  ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());

  profile_manager_ = new testing::ProfileManager(profiles_dir_.path());
  browser_process_->SetProfileManager(profile_manager_);  // Takes ownership.

  called_set_up_ = true;
}
