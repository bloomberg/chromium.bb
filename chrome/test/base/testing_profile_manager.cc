// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace testing {

class ProfileManager : public ::ProfileManager {
 public:
  explicit ProfileManager(const FilePath& user_data_dir)
      : ::ProfileManager(user_data_dir) {}

 protected:
  virtual Profile* CreateProfileHelper(const FilePath& file_path) OVERRIDE {
    return new TestingProfile(file_path);
  }
};

}  // namespace testing

TestingProfileManager::TestingProfileManager(TestingBrowserProcess* process)
    : called_set_up_(false),
      browser_process_(process),
      local_state_(process) {
}

TestingProfileManager::~TestingProfileManager() {
}

bool TestingProfileManager::SetUp() {
  SetUpInternal();
  return called_set_up_;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& profile_name,
    const string16& user_name,
    int avatar_id) {
  DCHECK(called_set_up_);

  // Create a path for the profile based on the name.
  FilePath profile_path(profiles_dir_.path());
  profile_path = profile_path.AppendASCII(profile_name);

  // Create the profile and register it.
  TestingProfile* profile = new TestingProfile(profile_path);
  profile_manager_->AddProfile(profile);  // Takes ownership.

  // Update the user metadata.
  ProfileInfoCache& cache = profile_manager_->GetProfileInfoCache();
  size_t index = cache.GetIndexOfProfileWithPath(profile_path);
  cache.SetNameOfProfileAtIndex(index, user_name);
  cache.SetAvatarIconOfProfileAtIndex(index, avatar_id);

  testing_profiles_.insert(std::make_pair(profile_name, profile));

  return profile;
}

TestingProfile* TestingProfileManager::CreateTestingProfile(
    const std::string& name) {
  DCHECK(called_set_up_);
  return CreateTestingProfile(name, UTF8ToUTF16(name), 0);
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

ProfileManager* TestingProfileManager::profile_manager() {
  DCHECK(called_set_up_);
  return profile_manager_;
}

ProfileInfoCache* TestingProfileManager::profile_info_cache() {
  DCHECK(called_set_up_);
  return &profile_manager_->GetProfileInfoCache();
}

void TestingProfileManager::DeleteProfileInfoCache() {
  profile_manager_->profile_info_cache_.reset(NULL);
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
