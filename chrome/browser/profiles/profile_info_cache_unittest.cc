// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

class ProfileInfoCacheUnittests : public testing::Test {
 protected:
  ProfileInfoCacheUnittests()
      : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
    cache_.reset(new ProfileInfoCache(local_state_.Get(), GetUserDataDir()));
  }

  FilePath GetUserDataDir() {
    return StringToFilePath("usr").Append(StringToFilePath("profile"));
  }

  FilePath StringToFilePath(std::string string_path) {
#if defined(OS_POSIX)
    return FilePath(string_path);
#elif defined(OS_WIN)
    return FilePath(ASCIIToWide(string_path));
#endif
  }

  scoped_ptr<ProfileInfoCache> cache_;
  ScopedTestingLocalState local_state_;
};

// TODO(sail): This fails on windows for some reason.
TEST_F(ProfileInfoCacheUnittests, DISABLED_AddProfiles) {
  EXPECT_EQ(0u, cache_->GetNumberOfProfiles());

  for (uint32 i = 0; i < 4; ++i) {
    std::string base_name = StringPrintf("path_%ud", i);
    FilePath profile_path =
        GetUserDataDir().Append(StringToFilePath(base_name));
    string16 profile_name = ASCIIToUTF16(StringPrintf("name_%ud", i));
    const SkBitmap& icon = ResourceBundle::GetSharedInstance().GetImageNamed(
        ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(i));

    cache_->AddProfileToCache(profile_path, profile_name, 0);

    EXPECT_EQ(i + 1, cache_->GetNumberOfProfiles());
    EXPECT_EQ(profile_name, cache_->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(profile_path, cache_->GetPathOfProfileAtIndex(i));
    const SkBitmap& actual_icon = cache_->GetAvatarIconOfProfileAtIndex(i);
    EXPECT_EQ(icon.width(), actual_icon.width());
    EXPECT_EQ(icon.height(), actual_icon.height());
  }
}

TEST_F(ProfileInfoCacheUnittests, DISABLED_DeleteProfile) {
  EXPECT_EQ(0u, cache_->GetNumberOfProfiles());

  FilePath path_1 = GetUserDataDir().Append(StringToFilePath("path_1"));
  cache_->AddProfileToCache(path_1, ASCIIToUTF16("name_1"),
                            0);
  EXPECT_EQ(1u, cache_->GetNumberOfProfiles());

  FilePath path_2 = GetUserDataDir().Append(StringToFilePath("path_2"));
  string16 name_2 = ASCIIToUTF16("name_2");
  cache_->AddProfileToCache(path_2, name_2, 0);
  EXPECT_EQ(2u, cache_->GetNumberOfProfiles());

  cache_->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, cache_->GetNumberOfProfiles());
  EXPECT_EQ(name_2, cache_->GetNameOfProfileAtIndex(0));

  cache_->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, cache_->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheUnittests, DISABLED_MutateProfile) {
  cache_->AddProfileToCache(GetUserDataDir().Append(StringToFilePath("path_1")),
                           ASCIIToUTF16("name_1"), 0);
  cache_->AddProfileToCache(GetUserDataDir().Append(StringToFilePath("path_2")),
                           ASCIIToUTF16("name_2"), 0);

  string16 new_name = ASCIIToUTF16("new_name");
  cache_->SetNameOfProfileAtIndex(1, new_name);
  EXPECT_EQ(new_name, cache_->GetNameOfProfileAtIndex(1));
  EXPECT_NE(new_name, cache_->GetNameOfProfileAtIndex(0));

  size_t new_icon_index = 3;
  cache_->SetAvatarIconOfProfileAtIndex(1, new_icon_index);
  // Not much to test.
  cache_->GetAvatarIconOfProfileAtIndex(1);
}

} // namespace
