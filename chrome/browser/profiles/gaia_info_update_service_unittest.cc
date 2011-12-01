// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_info_cache_unittest.h"
#include "chrome/common/pref_names.h"
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
  MOCK_CONST_METHOD0(GetProfilePictureStatus,
                     ProfileDownloader::PictureStatus());
  MOCK_CONST_METHOD0(GetProfilePictureURL, std::string());
};

class GAIAInfoUpdateServiceMock : public GAIAInfoUpdateService {
 public:
  explicit GAIAInfoUpdateServiceMock(Profile* profile)
      : GAIAInfoUpdateService(profile) {
  }

  virtual ~GAIAInfoUpdateServiceMock() {
  }

  MOCK_METHOD0(Update, void());
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

} // namespace

TEST_F(GAIAInfoUpdateServiceTest, DownloadSuccess) {
  GAIAInfoUpdateService service(profile());
  NiceMock<ProfileDownloaderMock> downloader(&service);

  string16 name = ASCIIToUTF16("Pat Smith");
  EXPECT_CALL(downloader, GetProfileFullName()).WillOnce(Return(name));
  gfx::Image image = gfx::test::CreateImage();
  SkBitmap bmp = image;
  EXPECT_CALL(downloader, GetProfilePicture()).WillOnce(Return(bmp));
  EXPECT_CALL(downloader, GetProfilePictureStatus()).
      WillOnce(Return(ProfileDownloader::PICTURE_SUCCESS));
  std::string url("foo.com");
  EXPECT_CALL(downloader, GetProfilePictureURL()).WillOnce(Return(url));

  // No URL should be cached yet.
  EXPECT_EQ(std::string(), service.GetCachedPictureURL());

  service.OnDownloadComplete(&downloader, true);

  // On success both the profile info and GAIA info should be updated.
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, *GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
  EXPECT_EQ(url, service.GetCachedPictureURL());
}

TEST_F(GAIAInfoUpdateServiceTest, DownloadFailure) {
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  string16 old_name = GetCache()->GetNameOfProfileAtIndex(index);
  gfx::Image old_image = GetCache()->GetAvatarIconOfProfileAtIndex(index);

  GAIAInfoUpdateService service(profile());
  EXPECT_EQ(std::string(), service.GetCachedPictureURL());
  NiceMock<ProfileDownloaderMock> downloader(&service);

  service.OnDownloadComplete(&downloader, false);

  // On failure nothing should be updated.
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(old_name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(string16(), GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(index));
  EXPECT_EQ(std::string(), service.GetCachedPictureURL());
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
  EXPECT_CALL(downloader, GetProfilePictureStatus()).
      WillOnce(Return(ProfileDownloader::PICTURE_SUCCESS));
  EXPECT_CALL(downloader, GetProfilePictureURL()).WillOnce(Return(""));

  service.OnDownloadComplete(&downloader, true);

  // On success with no migration the profile info should not be updated but
  // the GAIA info should be updated.
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(index));
  EXPECT_EQ(old_name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(new_name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_TRUE(gfx::test::IsEqual(
      new_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
}

TEST_F(GAIAInfoUpdateServiceTest, ShouldUseGAIAProfileInfo) {
  bool sync_enabled = profile()->GetOriginalProfile()->IsSyncAccessible();
  EXPECT_EQ(sync_enabled,
            GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile()));
}

TEST_F(GAIAInfoUpdateServiceTest, ScheduleUpdate) {
  GAIAInfoUpdateService service(profile());
  EXPECT_TRUE(service.timer_.IsRunning());
  service.timer_.Stop();
  EXPECT_FALSE(service.timer_.IsRunning());
  service.ScheduleNextUpdate();
  EXPECT_TRUE(service.timer_.IsRunning());
}

TEST_F(GAIAInfoUpdateServiceTest, LogOut) {
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   "pat@example.com");
  string16 gaia_name = UTF8ToUTF16("Pat Foo");
  GetCache()->SetGAIANameOfProfileAtIndex(0, gaia_name);
  gfx::Image gaia_picture = gfx::test::CreateImage();
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, &gaia_picture);

  GAIAInfoUpdateService service(profile());
  // Log out.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "");

  // Verify that the GAIA name and picture are unset.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(0).empty());
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
}

TEST_F(GAIAInfoUpdateServiceTest, LogIn) {
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "");
  GAIAInfoUpdateServiceMock service(profile());

  // Log in.
  EXPECT_CALL(service, Update());
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                   "pat@example.com");
}
