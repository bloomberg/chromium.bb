// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache_unittest.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using ::testing::Return;
using ::testing::NiceMock;

namespace {

class ProfileDownloaderMock : public ProfileDownloader {
 public:
  explicit ProfileDownloaderMock(ProfileDownloaderDelegate* delegate)
      : ProfileDownloader(delegate) {
  }

  virtual ~ProfileDownloaderMock() {
  }

  MOCK_CONST_METHOD0(GetProfileFullName, string16());
  MOCK_CONST_METHOD0(GetProfilePicture, SkBitmap());
};

class GAIAInfoUpdateServiceTest : public ProfileInfoCacheTest {
 protected:
  GAIAInfoUpdateServiceTest() : profile_(NULL) {
  }

  Profile* profile() {
    if (!profile_)
      profile_ = testing_profile_manager_.CreateTestingProfile("profile_1");
    return profile_;
  }

 private:
  Profile* profile_;
};

TEST_F(GAIAInfoUpdateServiceTest, DownloadSuccess) {
  GAIAInfoUpdateService service(profile());
  NiceMock<ProfileDownloaderMock> downloader(&service);

  string16 name = ASCIIToUTF16("Pat Smith");
  EXPECT_CALL(downloader, GetProfileFullName()).WillOnce(Return(name));
  gfx::Image image = gfx::test::CreateImage();
  SkBitmap bmp = image;
  EXPECT_CALL(downloader, GetProfilePicture()).WillOnce(Return(bmp));

  service.OnDownloadComplete(&downloader, true);

  // On success both the profile info and GAIA info should be updated.
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
}

TEST_F(GAIAInfoUpdateServiceTest, DownloadFailure) {
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  string16 old_name = GetCache()->GetNameOfProfileAtIndex(index);
  gfx::Image old_image = GetCache()->GetAvatarIconOfProfileAtIndex(index);

  GAIAInfoUpdateService service(profile());
  NiceMock<ProfileDownloaderMock> downloader(&service);

  service.OnDownloadComplete(&downloader, false);

  // On failure nothing should be updated.
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(old_name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(string16(), GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_TRUE(gfx::test::IsEmpty(
      GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
}

TEST_F(GAIAInfoUpdateServiceTest, NoMigration) {
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  string16 old_name = GetCache()->GetNameOfProfileAtIndex(index);
  gfx::Image old_image = GetCache()->GetAvatarIconOfProfileAtIndex(index);

  // Mark the profile as migrated.
  GetCache()->SetHasMigratedToGAIAInfoOfProfileAtIndex(index, true);

  GAIAInfoUpdateService service(profile());
  NiceMock<ProfileDownloaderMock> downloader(&service);
  string16 new_name = ASCIIToUTF16("Pat Smith");
  EXPECT_CALL(downloader, GetProfileFullName()).WillOnce(Return(new_name));
  gfx::Image new_image = gfx::test::CreateImage();
  SkBitmap new_bmp = new_image;
  EXPECT_CALL(downloader, GetProfilePicture()).WillOnce(Return(new_bmp));

  service.OnDownloadComplete(&downloader, true);

  // On success with no migration the profile info should not be updated but
  // the GAIA info should be updated.
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(old_name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(new_name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_TRUE(gfx::test::IsEqual(
      new_image, GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
}

TEST_F(GAIAInfoUpdateServiceTest, ShouldUseGAIAProfileInfo) {
  // Currently this is disabled by default.
  EXPECT_FALSE(GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile()));
}

} // namespace
