// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/fake_profile_info_interface.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

FakeProfileInfo::FakeProfileInfo() {}
FakeProfileInfo::~FakeProfileInfo() {}

std::vector<AvatarMenuModel::Item*>* FakeProfileInfo::mock_profiles() {
 return &profiles_;
}

// static
gfx::Image& FakeProfileInfo::GetTestImage() {
  return ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_PROFILE_AVATAR_0);
}

size_t FakeProfileInfo::GetNumberOfProfiles() const {
  return profiles_.size();
}

size_t FakeProfileInfo::GetIndexOfProfileWithPath(
    const FilePath& profile_path) const {
  return std::string::npos;
}

string16 FakeProfileInfo::GetNameOfProfileAtIndex(size_t index) const {
  return profiles_[index]->name;
}

FilePath FakeProfileInfo::GetPathOfProfileAtIndex(size_t index) const {
  return FilePath();
}

const gfx::Image& FakeProfileInfo::GetAvatarIconOfProfileAtIndex(
    size_t index) const {
  return profiles_[index]->icon;
}
