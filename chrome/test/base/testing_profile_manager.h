// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"

class PrefServiceSyncable;
class ProfileInfoCache;
class ProfileManager;
class TestingBrowserProcess;
class TestingProfile;

// The TestingProfileManager is a TestingProfile factory for a multi-profile
// environment. It will bring up a full ProfileManager and attach it to the
// TestingBrowserProcess set up in your test.
//
// When a Profile is needed for testing, create it through the factory method
// below instead of creating it via |new TestingProfile|. It is not possible
// to register profiles created in that fashion with the ProfileManager.
class TestingProfileManager {
 public:
  explicit TestingProfileManager(TestingBrowserProcess* browser_process);
  ~TestingProfileManager();

  // This needs to be called in testing::Test::SetUp() to put the object in a
  // valid state. Some work cannot be done in a constructor because it may
  // call gtest asserts to verify setup. The result of this call can be used
  // to ASSERT before doing more SetUp work in the test.
  bool SetUp() WARN_UNUSED_RESULT;

  // Creates a new TestingProfile whose data lives in a directory related to
  // profile_name, which is a non-user-visible key for the test environment.
  // |prefs| is the PrefService used by the profile. If it is NULL, the profile
  // creates a PrefService on demand.
  // |user_name|, |avatar_id| and |supervised_user_id| are passed along to the
  // ProfileInfoCache and provide the user-visible profile metadata. This will
  // register the TestingProfile with the profile subsystem as well. The
  // subsystem owns the Profile and returns a weak pointer.
  // |factories| contains BCKSs to use with the newly created profile.
  TestingProfile* CreateTestingProfile(
      const std::string& profile_name,
      scoped_ptr<PrefServiceSyncable> prefs,
      const base::string16& user_name,
      int avatar_id,
      const std::string& supervised_user_id,
      const TestingProfile::TestingFactories& factories);

  // Small helper for creating testing profiles. Just forwards to above.
  TestingProfile* CreateTestingProfile(const std::string& name);

  // Creates a new guest TestingProfile whose data lives in the guest profile
  // test environment directory, as specified by the profile manager.
  // This profile will not be added to the ProfileInfoCache. This will
  // register the TestingProfile with the profile subsystem as well.
  // The subsystem owns the Profile and returns a weak pointer.
  TestingProfile* CreateGuestProfile();

  // Deletes a TestingProfile from the profile subsystem.
  void DeleteTestingProfile(const std::string& profile_name);

  // Deletes all TestingProfiles from the profile subsystem, including guest
  // profiles.
  void DeleteAllTestingProfiles();

  // Deletes a guest TestingProfile from the profile manager.
  void DeleteGuestProfile();

  // Deletes the cache instance. This is useful for testing that the cache is
  // properly persisting data.
  void DeleteProfileInfoCache();

  // Sets ProfileManager's logged_in state. This is only useful on ChromeOS.
  void SetLoggedIn(bool logged_in);

  // Helper accessors.
  const base::FilePath& profiles_dir();
  ProfileManager* profile_manager();
  ProfileInfoCache* profile_info_cache();

 private:
  typedef std::map<std::string, TestingProfile*> TestingProfilesMap;

  // Does the actual ASSERT-checked SetUp work. This function cannot have a
  // return value, so it sets the |called_set_up_| flag on success and that is
  // returned in the public SetUp.
  void SetUpInternal();

  // Whether SetUp() was called to put the object in a valid state.
  bool called_set_up_;

  // The directory in which new profiles are placed.
  base::ScopedTempDir profiles_dir_;

  // Weak reference to the browser process on which the ProfileManager is set.
  TestingBrowserProcess* browser_process_;

  // Local state in which all the profiles are registered.
  ScopedTestingLocalState local_state_;

  // Weak reference to the profile manager.
  ProfileManager* profile_manager_;

  // Map of profile_name to TestingProfile* from CreateTestingProfile().
  TestingProfilesMap testing_profiles_;

  DISALLOW_COPY_AND_ASSIGN(TestingProfileManager);
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
