// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_

#include <map>

#include "base/compiler_specific.h"
#include "chrome/browser/profiles/fake_profile_info_interface.h"
#include "chrome/browser/profiles/profile_manager.h"

class ProfileInfoCache;
class TestingProfile;

// The TestingProfileManager is used whenever a ProfileManager is needed in a
// test environment. It overrides only GetProfileInfo() and friends, but at
// some point in the future it may make use of the TestingProfiles it holds.
class TestingProfileManager : public ProfileManager {
 public:
  typedef std::map<TestingProfile*, AvatarMenuModel::Item*> TestingProfiles;

  TestingProfileManager();
  virtual ~TestingProfileManager();

  // Creates an AvatarMenuModel::Item for use in AddTestingProfile(). Caller
  // owns the result.
  static AvatarMenuModel::Item* CreateFakeInfo(
      std::string name) WARN_UNUSED_RESULT;

  // Adds a TestingProfile with an AvatarMenuModel::Item. The |info| is used by
  // the FakeProfileInfo, while the TestingProfile is merely a way to key this
  // information in your test. The |profile| is unused otherwise.
  void AddTestingProfile(TestingProfile* profile, AvatarMenuModel::Item* info);

  // Removes a profile and the corresponding AvatarMenuModel::Item from the
  // |fake_profile_info_|.
  void RemoveTestingProfile(TestingProfile* profile);

  const TestingProfiles& testing_profiles() {
    return testing_profiles_;
  }

  FakeProfileInfo& fake_profile_info() {
    return fake_profile_info_;
  }

  // ProfileManager:
  virtual ProfileInfoInterface& GetProfileInfo();
  virtual ProfileInfoCache& GetMutableProfileInfo();

 private:
  TestingProfiles testing_profiles_;

  FakeProfileInfo fake_profile_info_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_MANAGER_H_
