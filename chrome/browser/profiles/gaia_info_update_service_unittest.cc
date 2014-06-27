// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/gaia_info_update_service.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_cache_unittest.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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

  MOCK_CONST_METHOD0(GetProfileFullName, base::string16());
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
    if (!profile_) {
      profile_ = testing_profile_manager_.CreateTestingProfile("Person 1");
      // The testing manager sets the profile name manually, which counts as
      // a user-customized profile name. Reset this to match the default name
      // we are actually using.
      size_t index = GetCache()->GetIndexOfProfileWithPath(profile_->GetPath());
      GetCache()->SetProfileIsUsingDefaultNameAtIndex(index, true);
    }
    return profile_;
  }

  NiceMock<GAIAInfoUpdateServiceMock>* service() { return service_.get(); }
  NiceMock<ProfileDownloaderMock>* downloader() { return downloader_.get(); }

 private:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  Profile* profile_;
  scoped_ptr<NiceMock<GAIAInfoUpdateServiceMock> > service_;
  scoped_ptr<NiceMock<ProfileDownloaderMock> > downloader_;
};

void GAIAInfoUpdateServiceTest::SetUp() {
  ProfileInfoCacheTest::SetUp();
  service_.reset(new NiceMock<GAIAInfoUpdateServiceMock>(profile()));
  downloader_.reset(new NiceMock<ProfileDownloaderMock>(service()));
}

void GAIAInfoUpdateServiceTest::TearDown() {
  downloader_.reset();
  service_->Shutdown();
  service_.reset();
  ProfileInfoCacheTest::TearDown();
}

} // namespace

TEST_F(GAIAInfoUpdateServiceTest, DownloadSuccess) {
  base::string16 name = base::ASCIIToUTF16("Pat Smith");
  EXPECT_CALL(*downloader(), GetProfileFullName()).WillOnce(Return(name));
  gfx::Image image = gfx::test::CreateImage();
  const SkBitmap* bmp = image.ToSkBitmap();
  EXPECT_CALL(*downloader(), GetProfilePicture()).WillOnce(Return(*bmp));
  EXPECT_CALL(*downloader(), GetProfilePictureStatus()).
      WillOnce(Return(ProfileDownloader::PICTURE_SUCCESS));
  std::string url("foo.com");
  EXPECT_CALL(*downloader(), GetProfilePictureURL()).WillOnce(Return(url));

  // No URL should be cached yet.
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());

  service()->OnProfileDownloadSuccess(downloader());

  // On success both the profile info and GAIA info should be updated.
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  EXPECT_EQ(name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      image, *GetCache()->GetGAIAPictureOfProfileAtIndex(index)));
  EXPECT_EQ(url, service()->GetCachedPictureURL());
}

TEST_F(GAIAInfoUpdateServiceTest, DownloadFailure) {
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  base::string16 old_name = GetCache()->GetNameOfProfileAtIndex(index);
  gfx::Image old_image = GetCache()->GetAvatarIconOfProfileAtIndex(index);

  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());

  service()->OnProfileDownloadFailure(downloader(),
                                    ProfileDownloaderDelegate::SERVICE_ERROR);

  // On failure nothing should be updated.
  EXPECT_EQ(old_name, GetCache()->GetNameOfProfileAtIndex(index));
  EXPECT_EQ(base::string16(), GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(index));
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());
}

TEST_F(GAIAInfoUpdateServiceTest, ShouldUseGAIAProfileInfo) {
#if defined(OS_CHROMEOS)
  // This feature should never be enabled on ChromeOS.
  EXPECT_FALSE(GAIAInfoUpdateService::ShouldUseGAIAProfileInfo(profile()));
#endif
}

TEST_F(GAIAInfoUpdateServiceTest, ScheduleUpdate) {
  EXPECT_TRUE(service()->timer_.IsRunning());
  service()->timer_.Stop();
  EXPECT_FALSE(service()->timer_.IsRunning());
  service()->ScheduleNextUpdate();
  EXPECT_TRUE(service()->timer_.IsRunning());
}

#if !defined(OS_CHROMEOS)

TEST_F(GAIAInfoUpdateServiceTest, LogOut) {
  SigninManager* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->SetAuthenticatedUsername("pat@example.com");
  base::string16 gaia_name = base::UTF8ToUTF16("Pat Foo");
  GetCache()->SetGAIANameOfProfileAtIndex(0, gaia_name);
  gfx::Image gaia_picture = gfx::test::CreateImage();
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, &gaia_picture);

  // Set a fake picture URL.
  profile()->GetPrefs()->SetString(prefs::kProfileGAIAInfoPictureURL,
                                   "example.com");

  EXPECT_FALSE(service()->GetCachedPictureURL().empty());

  // Log out.
  signin_manager->SignOut(signin_metrics::SIGNOUT_TEST);
  // Verify that the GAIA name and picture, and picture URL are unset.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(0).empty());
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_TRUE(service()->GetCachedPictureURL().empty());
}

TEST_F(GAIAInfoUpdateServiceTest, LogIn) {
  // Log in.
  EXPECT_CALL(*service(), Update());
  SigninManager* signin_manager =
      SigninManagerFactory::GetForProfile(profile());
  signin_manager->OnExternalSigninCompleted("pat@example.com");
}

#endif
