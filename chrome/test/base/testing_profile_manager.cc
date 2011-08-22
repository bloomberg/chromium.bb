// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_manager.h"

#include <algorithm>

#include "base/utf_string_conversions.h"

TestingProfileManager::TestingProfileManager() {
}

TestingProfileManager::~TestingProfileManager() {
}

// static
AvatarMenuModel::Item* TestingProfileManager::CreateFakeInfo(std::string name) {
  AvatarMenuModel::Item* item =
      new AvatarMenuModel::Item(0, FakeProfileInfo::GetTestImage());
  item->name = UTF8ToUTF16(name);
  return item;
}

void TestingProfileManager::AddTestingProfile(TestingProfile* profile,
                                              AvatarMenuModel::Item* info) {
  testing_profiles_.insert(std::make_pair(profile, info));
  fake_profile_info_.mock_profiles()->push_back(info);
}

void TestingProfileManager::RemoveTestingProfile(TestingProfile* profile) {
  std::map<TestingProfile*, AvatarMenuModel::Item*>::iterator it(
      testing_profiles_.find(profile));
  DCHECK(it != testing_profiles_.end());

  AvatarMenuModel::Item* info = it->second;
  std::vector<AvatarMenuModel::Item*>* info_list =
      fake_profile_info_.mock_profiles();
  std::vector<AvatarMenuModel::Item*>::iterator info_it(
      std::find(info_list->begin(), info_list->end(), info));
  DCHECK(info_it != fake_profile_info_.mock_profiles()->end());

  testing_profiles_.erase(it);
  fake_profile_info_.mock_profiles()->erase(info_it);
}

ProfileInfoInterface& TestingProfileManager::GetProfileInfo() {
  return fake_profile_info_;
}

ProfileInfoCache& TestingProfileManager::GetMutableProfileInfo() {
  NOTIMPLEMENTED();
  return ProfileManager::GetMutableProfileInfo();
}
