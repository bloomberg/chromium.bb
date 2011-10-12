// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

class ProfileInfoCacheUnittests : public testing::Test {
 protected:
  ProfileInfoCacheUnittests()
      : testing_profile_manager_(
          static_cast<TestingBrowserProcess*>(g_browser_process)) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  ProfileInfoCache* GetCache() {
    return testing_profile_manager_.profile_info_cache();
  }

  const FilePath& GetUserDataDir() {
    return testing_profile_manager_.profile_manager()->user_data_dir();
  }

  FilePath StringToFilePath(std::string string_path) {
#if defined(OS_POSIX)
    return FilePath(string_path);
#elif defined(OS_WIN)
    return FilePath(ASCIIToWide(string_path));
#endif
  }

 private:
  TestingProfileManager testing_profile_manager_;
};

TEST_F(ProfileInfoCacheUnittests, AddProfiles) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  for (uint32 i = 0; i < 4; ++i) {
    std::string base_name = StringPrintf("path_%ud", i);
    FilePath profile_path =
        GetUserDataDir().Append(StringToFilePath(base_name));
    string16 profile_name = ASCIIToUTF16(StringPrintf("name_%ud", i));
    const SkBitmap& icon = ResourceBundle::GetSharedInstance().GetImageNamed(
        ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(i));

    GetCache()->AddProfileToCache(profile_path, profile_name, string16(), 0);

    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
    const SkBitmap& actual_icon = GetCache()->GetAvatarIconOfProfileAtIndex(i);
    EXPECT_EQ(icon.width(), actual_icon.width());
    EXPECT_EQ(icon.height(), actual_icon.height());
  }
}

TEST_F(ProfileInfoCacheUnittests, DeleteProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  FilePath path_1 = GetUserDataDir().Append(StringToFilePath("path_1"));
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"), string16(),
                            0);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  FilePath path_2 = GetUserDataDir().Append(StringToFilePath("path_2"));
  string16 name_2 = ASCIIToUTF16("name_2");
  GetCache()->AddProfileToCache(path_2, name_2, string16(), 0);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  GetCache()->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameOfProfileAtIndex(0));

  GetCache()->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheUnittests, MutateProfile) {
  GetCache()->AddProfileToCache(GetUserDataDir().Append(
      StringToFilePath("path_1")), ASCIIToUTF16("name_1"), string16(), 0);
  GetCache()->AddProfileToCache(GetUserDataDir().Append(
      StringToFilePath("path_2")), ASCIIToUTF16("name_2"), string16(), 0);

  string16 new_name = ASCIIToUTF16("new_name");
  GetCache()->SetNameOfProfileAtIndex(1, new_name);
  EXPECT_EQ(new_name, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_NE(new_name, GetCache()->GetNameOfProfileAtIndex(0));

  string16 new_user_name = ASCIIToUTF16("user_name");
  GetCache()->SetUserNameOfProfileAtIndex(1, new_user_name);
  EXPECT_EQ(new_user_name, GetCache()->GetUserNameOfProfileAtIndex(1));
  EXPECT_NE(new_user_name, GetCache()->GetUserNameOfProfileAtIndex(0));

  size_t new_icon_index = 3;
  GetCache()->SetAvatarIconOfProfileAtIndex(1, new_icon_index);
  // Not much to test.
  GetCache()->GetAvatarIconOfProfileAtIndex(1);
}

TEST_F(ProfileInfoCacheUnittests, BackgroundModeStatus) {
  GetCache()->AddProfileToCache(
      GetUserDataDir().Append(StringToFilePath("path_1")),
      ASCIIToUTF16("name_1"), string16(), 0);
  GetCache()->AddProfileToCache(
      GetUserDataDir().Append(StringToFilePath("path_2")),
      ASCIIToUTF16("name_2"), string16(), 0);

  EXPECT_FALSE(GetCache()->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->GetBackgroundStatusOfProfileAtIndex(1));

  GetCache()->SetBackgroundStatusOfProfileAtIndex(1, true);

  EXPECT_FALSE(GetCache()->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_TRUE(GetCache()->GetBackgroundStatusOfProfileAtIndex(1));

  GetCache()->SetBackgroundStatusOfProfileAtIndex(0, true);

  EXPECT_TRUE(GetCache()->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_TRUE(GetCache()->GetBackgroundStatusOfProfileAtIndex(1));

  GetCache()->SetBackgroundStatusOfProfileAtIndex(1, false);

  EXPECT_TRUE(GetCache()->GetBackgroundStatusOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->GetBackgroundStatusOfProfileAtIndex(1));
}

}  // namespace
