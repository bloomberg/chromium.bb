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
  MOCK_CONST_METHOD0(GetProfileGivenName, base::string16());
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
      profile_ = CreateProfile("Person 1");
    return profile_;
  }

  NiceMock<GAIAInfoUpdateServiceMock>* service() { return service_.get(); }
  NiceMock<ProfileDownloaderMock>* downloader() { return downloader_.get(); }

  Profile* CreateProfile(const std::string& name) {
    Profile* profile = testing_profile_manager_.CreateTestingProfile(name);
    // The testing manager sets the profile name manually, which counts as
    // a user-customized profile name. Reset this to match the default name
    // we are actually using.
    size_t index = GetCache()->GetIndexOfProfileWithPath(profile->GetPath());
    GetCache()->SetProfileIsUsingDefaultNameAtIndex(index, true);
    return profile;
  }

  static std::string GivenName(const std::string& id) {
    return id + "first";
  }
  static std::string FullName(const std::string& id) {
    return GivenName(id) + " " + id + "last";
  }
  static base::string16 GivenName16(const std::string& id) {
    return base::UTF8ToUTF16(GivenName(id));
  }
  static base::string16 FullName16(const std::string& id) {
    return base::UTF8ToUTF16(FullName(id));
  }

  void ProfileDownloadSuccess(
      const base::string16& full_name,
      const base::string16& given_name,
      const gfx::Image& image,
      const std::string& url) {
    EXPECT_CALL(*downloader(), GetProfileFullName()).
        WillOnce(Return(full_name));
    EXPECT_CALL(*downloader(), GetProfileGivenName()).
        WillOnce(Return(given_name));
    const SkBitmap* bmp = image.ToSkBitmap();
    EXPECT_CALL(*downloader(), GetProfilePicture()).WillOnce(Return(*bmp));
    EXPECT_CALL(*downloader(), GetProfilePictureStatus()).
        WillOnce(Return(ProfileDownloader::PICTURE_SUCCESS));
    EXPECT_CALL(*downloader(), GetProfilePictureURL()).WillOnce(Return(url));

    service()->OnProfileDownloadSuccess(downloader());
  }

  void RenameProfile(const base::string16& full_name,
                     const base::string16& given_name) {
    gfx::Image image = gfx::test::CreateImage();
    std::string url("foo.com");
    ProfileDownloadSuccess(full_name, given_name, image, url);

    // Make sure the right profile was updated correctly.
    size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
    EXPECT_EQ(full_name, GetCache()->GetGAIANameOfProfileAtIndex(index));
    EXPECT_EQ(given_name, GetCache()->GetGAIAGivenNameOfProfileAtIndex(index));
  }

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
  // No URL should be cached yet.
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());

  base::string16 name = base::ASCIIToUTF16("Pat Smith");
  base::string16 given_name = base::ASCIIToUTF16("Pat");
  gfx::Image image = gfx::test::CreateImage();
  std::string url("foo.com");
  ProfileDownloadSuccess(name, given_name, image, url);

  // On success the GAIA info should be updated.
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  EXPECT_EQ(name, GetCache()->GetGAIANameOfProfileAtIndex(index));
  EXPECT_EQ(given_name, GetCache()->GetGAIAGivenNameOfProfileAtIndex(index));
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
  EXPECT_EQ(base::string16(),
            GetCache()->GetGAIAGivenNameOfProfileAtIndex(index));
  EXPECT_TRUE(gfx::test::IsEqual(
      old_image, GetCache()->GetAvatarIconOfProfileAtIndex(index)));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(index));
  EXPECT_EQ(std::string(), service()->GetCachedPictureURL());
}

TEST_F(GAIAInfoUpdateServiceTest, HandlesProfileReordering) {
  size_t index = GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());
  GetCache()->SetNameOfProfileAtIndex(index, FullName16("B"));
  GetCache()->SetProfileIsUsingDefaultNameAtIndex(index, true);

  CreateProfile(FullName("A"));
  CreateProfile(FullName("C"));
  CreateProfile(FullName("E"));

  size_t index_before =
      GetCache()->GetIndexOfProfileWithPath(profile()->GetPath());

  // Rename our profile.
  RenameProfile(FullName16("D"), GivenName16("D"));
  // Profiles should have been reordered in the cache.
  EXPECT_NE(index_before,
            GetCache()->GetIndexOfProfileWithPath(profile()->GetPath()));
  // Rename the profile back to the original name, it should go back to its
  // original position.
  RenameProfile(FullName16("B"), GivenName16("B"));
  EXPECT_EQ(index_before,
            GetCache()->GetIndexOfProfileWithPath(profile()->GetPath()));

  // Rename only the given name of our profile.
  RenameProfile(FullName16("B"), GivenName16("D"));
  // Rename the profile back to the original name, it should go back to its
  // original position.
  RenameProfile(FullName16("B"), GivenName16("B"));
  EXPECT_EQ(index_before,
            GetCache()->GetIndexOfProfileWithPath(profile()->GetPath()));

  // Rename only the full name of our profile.
  RenameProfile(FullName16("D"), GivenName16("B"));
  // Rename the profile back to the original name, it should go back to its
  // original position.
  RenameProfile(FullName16("B"), GivenName16("B"));
  EXPECT_EQ(index_before,
            GetCache()->GetIndexOfProfileWithPath(profile()->GetPath()));
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
