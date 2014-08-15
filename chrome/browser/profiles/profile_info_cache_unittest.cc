// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache_unittest.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile_avatar_downloader.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using content::BrowserThread;

ProfileNameVerifierObserver::ProfileNameVerifierObserver(
    TestingProfileManager* testing_profile_manager)
    : testing_profile_manager_(testing_profile_manager) {
  DCHECK(testing_profile_manager_);
}

ProfileNameVerifierObserver::~ProfileNameVerifierObserver() {
}

void ProfileNameVerifierObserver::OnProfileAdded(
    const base::FilePath& profile_path) {
  base::string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) == profile_names_.end());
  profile_names_.insert(profile_name);
}

void ProfileNameVerifierObserver::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  base::string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) != profile_names_.end());
  profile_names_.erase(profile_name);
}

void ProfileNameVerifierObserver::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  EXPECT_TRUE(profile_names_.find(profile_name) == profile_names_.end());
}

void ProfileNameVerifierObserver::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  base::string16 new_profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(old_profile_name) != profile_names_.end());
  EXPECT_TRUE(profile_names_.find(new_profile_name) == profile_names_.end());
  profile_names_.erase(old_profile_name);
  profile_names_.insert(new_profile_name);
}

void ProfileNameVerifierObserver::OnProfileAvatarChanged(
    const base::FilePath& profile_path) {
  base::string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) != profile_names_.end());
}

ProfileInfoCache* ProfileNameVerifierObserver::GetCache() {
  return testing_profile_manager_->profile_info_cache();
}

ProfileInfoCacheTest::ProfileInfoCacheTest()
    : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
      name_observer_(&testing_profile_manager_) {
}

ProfileInfoCacheTest::~ProfileInfoCacheTest() {
}

void ProfileInfoCacheTest::SetUp() {
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  testing_profile_manager_.profile_info_cache()->AddObserver(&name_observer_);
}

void ProfileInfoCacheTest::TearDown() {
  // Drain the UI thread to make sure all tasks are completed. This prevents
  // memory leaks.
  base::RunLoop().RunUntilIdle();
}

ProfileInfoCache* ProfileInfoCacheTest::GetCache() {
  return testing_profile_manager_.profile_info_cache();
}

base::FilePath ProfileInfoCacheTest::GetProfilePath(
    const std::string& base_name) {
  return testing_profile_manager_.profile_manager()->user_data_dir().
      AppendASCII(base_name);
}

void ProfileInfoCacheTest::ResetCache() {
  testing_profile_manager_.DeleteProfileInfoCache();
}

TEST_F(ProfileInfoCacheTest, AddProfiles) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (uint32 i = 0; i < 4; ++i) {
    base::FilePath profile_path =
        GetProfilePath(base::StringPrintf("path_%ud", i));
    base::string16 profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", i));
    const SkBitmap* icon = rb.GetImageNamed(
        profiles::GetDefaultAvatarIconResourceIDAtIndex(
            i)).ToSkBitmap();
    std::string supervised_user_id = i == 3 ? "TEST_ID" : "";

    GetCache()->AddProfileToCache(profile_path, profile_name, base::string16(),
                                  i, supervised_user_id);
    GetCache()->SetBackgroundStatusOfProfileAtIndex(i, true);
    base::string16 gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    GetCache()->SetGAIANameOfProfileAtIndex(i, gaia_name);

    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
    const SkBitmap* actual_icon =
        GetCache()->GetAvatarIconOfProfileAtIndex(i).ToSkBitmap();
    EXPECT_EQ(icon->width(), actual_icon->width());
    EXPECT_EQ(icon->height(), actual_icon->height());
    EXPECT_EQ(i == 3, GetCache()->ProfileIsSupervisedAtIndex(i));
    EXPECT_EQ(i == 3, GetCache()->IsOmittedProfileAtIndex(i));
    EXPECT_EQ(supervised_user_id,
              GetCache()->GetSupervisedUserIdOfProfileAtIndex(i));
  }

  // Reset the cache and test the it reloads correctly.
  ResetCache();

  EXPECT_EQ(4u, GetCache()->GetNumberOfProfiles());
  for (uint32 i = 0; i < 4; ++i) {
    base::FilePath profile_path =
          GetProfilePath(base::StringPrintf("path_%ud", i));
    EXPECT_EQ(i, GetCache()->GetIndexOfProfileWithPath(profile_path));
    base::string16 profile_name =
        ASCIIToUTF16(base::StringPrintf("name_%ud", i));
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(i, GetCache()->GetAvatarIconIndexOfProfileAtIndex(i));
    EXPECT_EQ(true, GetCache()->GetBackgroundStatusOfProfileAtIndex(i));
    base::string16 gaia_name = ASCIIToUTF16(base::StringPrintf("gaia_%ud", i));
    EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(i));
  }
}

TEST_F(ProfileInfoCacheTest, DeleteProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"),
                                base::string16(), 0, std::string());
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  base::FilePath path_2 = GetProfilePath("path_2");
  base::string16 name_2 = ASCIIToUTF16("name_2");
  GetCache()->AddProfileToCache(path_2, name_2, base::string16(), 0,
                                std::string());
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  GetCache()->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameOfProfileAtIndex(0));

  GetCache()->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheTest, MutateProfile) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"),
      base::string16(), 0, std::string());
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"),
      base::string16(), 0, std::string());

  base::string16 new_name = ASCIIToUTF16("new_name");
  GetCache()->SetNameOfProfileAtIndex(1, new_name);
  EXPECT_EQ(new_name, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_NE(new_name, GetCache()->GetNameOfProfileAtIndex(0));

  base::string16 new_user_name = ASCIIToUTF16("user_name");
  GetCache()->SetUserNameOfProfileAtIndex(1, new_user_name);
  EXPECT_EQ(new_user_name, GetCache()->GetUserNameOfProfileAtIndex(1));
  EXPECT_NE(new_user_name, GetCache()->GetUserNameOfProfileAtIndex(0));

  size_t new_icon_index = 3;
  GetCache()->SetAvatarIconOfProfileAtIndex(1, new_icon_index);
  // Not much to test.
  GetCache()->GetAvatarIconOfProfileAtIndex(1);
}

TEST_F(ProfileInfoCacheTest, Sort) {
  base::string16 name_a = ASCIIToUTF16("apple");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_a"), name_a, base::string16(), 0, std::string());

  base::string16 name_c = ASCIIToUTF16("cat");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_c"), name_c, base::string16(), 0, std::string());

  // Sanity check the initial order.
  EXPECT_EQ(name_a, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_EQ(name_c, GetCache()->GetNameOfProfileAtIndex(1));

  // Add a new profile (start with a capital to test case insensitive sorting.
  base::string16 name_b = ASCIIToUTF16("Banana");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_b"), name_b, base::string16(), 0, std::string());

  // Verify the new order.
  EXPECT_EQ(name_a, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_EQ(name_b, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_EQ(name_c, GetCache()->GetNameOfProfileAtIndex(2));

  // Change the name of an existing profile.
  name_a = UTF8ToUTF16("dog");
  GetCache()->SetNameOfProfileAtIndex(0, name_a);

  // Verify the new order.
  EXPECT_EQ(name_b, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_EQ(name_c, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_EQ(name_a, GetCache()->GetNameOfProfileAtIndex(2));

  // Delete a profile.
  GetCache()->DeleteProfileFromCache(GetProfilePath("path_c"));

  // Verify the new order.
  EXPECT_EQ(name_b, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_EQ(name_a, GetCache()->GetNameOfProfileAtIndex(1));
}

TEST_F(ProfileInfoCacheTest, BackgroundModeStatus) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"),
      base::string16(), 0, std::string());
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"),
      base::string16(), 0, std::string());

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

TEST_F(ProfileInfoCacheTest, ProfileActiveTime) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"),
      base::string16(), 0, std::string());
  EXPECT_EQ(base::Time(), GetCache()->GetProfileActiveTimeAtIndex(0));
  // Before & After times are artificially shifted because just relying upon
  // the system time can yield problems due to inaccuracies in the
  // underlying storage system (which uses a double with only 52 bits of
  // precision to store the 64-bit "time" number).  http://crbug.com/346827
  base::Time before = base::Time::Now();
  before -= base::TimeDelta::FromSeconds(1);
  GetCache()->SetProfileActiveTimeAtIndex(0);
  base::Time after = base::Time::Now();
  after += base::TimeDelta::FromSeconds(1);
  EXPECT_LE(before, GetCache()->GetProfileActiveTimeAtIndex(0));
  EXPECT_GE(after, GetCache()->GetProfileActiveTimeAtIndex(0));
}

TEST_F(ProfileInfoCacheTest, GAIAName) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("Person 1"),
      base::string16(), 0, std::string());
  base::string16 profile_name(ASCIIToUTF16("Person 2"));
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), profile_name, base::string16(), 0,
      std::string());

  int index1 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_1"));
  int index2 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_2"));

  // Sanity check.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index1).empty());
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index2).empty());

  // Set GAIA name. This re-sorts the cache.
  base::string16 gaia_name(ASCIIToUTF16("Pat Smith"));
  GetCache()->SetGAIANameOfProfileAtIndex(index2, gaia_name);
  index1 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_1"));
  index2 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_2"));

  // Since there is a GAIA name, we use that as a display name.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(index1).empty());
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(index2));
  EXPECT_EQ(gaia_name, GetCache()->GetNameOfProfileAtIndex(index2));

  // Don't use GAIA name as profile name. This re-sorts the cache.
  base::string16 custom_name(ASCIIToUTF16("Custom name"));
  GetCache()->SetNameOfProfileAtIndex(index2, custom_name);
  index1 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_1"));
  index2 = GetCache()->GetIndexOfProfileWithPath(GetProfilePath("path_2"));

  EXPECT_EQ(custom_name, GetCache()->GetNameOfProfileAtIndex(index2));
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(index2));
}

TEST_F(ProfileInfoCacheTest, GAIAPicture) {
  const int kDefaultAvatarIndex = 0;
  const int kOtherAvatarIndex = 1;
  const int kGaiaPictureSize = 256;  // Standard size of a Gaia account picture.
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"),
      base::string16(), kDefaultAvatarIndex, std::string());
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"),
      base::string16(), kDefaultAvatarIndex, std::string());

  // Sanity check.
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));

  // The profile icon should be the default one.
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(0));
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  int default_avatar_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(kDefaultAvatarIndex);
  const gfx::Image& default_avatar_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(default_avatar_id));
  EXPECT_TRUE(gfx::test::IsEqual(
      default_avatar_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Set GAIA picture.
  gfx::Image gaia_image(gfx::test::CreateImage(
      kGaiaPictureSize, kGaiaPictureSize));
  GetCache()->SetGAIAPictureOfProfileAtIndex(1, &gaia_image);
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  // Since we're still using the default avatar, the GAIA image should be
  // preferred over the generic avatar image.
  EXPECT_TRUE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Set another avatar. This should make it preferred over the GAIA image.
  GetCache()->SetAvatarIconOfProfileAtIndex(1, kOtherAvatarIndex);
  EXPECT_FALSE(GetCache()->ProfileIsUsingDefaultAvatarAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  int other_avatar_id =
      profiles::GetDefaultAvatarIconResourceIDAtIndex(kOtherAvatarIndex);
  const gfx::Image& other_avatar_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(other_avatar_id));
  EXPECT_TRUE(gfx::test::IsEqual(
      other_avatar_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Explicitly setting the GAIA picture should make it preferred again.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, true);
  EXPECT_TRUE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Clearing the IsUsingGAIAPicture flag should result in the generic image
  // being used again.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, false);
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::IsEqual(
      other_avatar_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));
}

TEST_F(ProfileInfoCacheTest, PersistGAIAPicture) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"),
      base::string16(), 0, std::string());
  gfx::Image gaia_image(gfx::test::CreateImage());

  content::WindowedNotificationObserver save_observer(
      chrome::NOTIFICATION_PROFILE_CACHE_PICTURE_SAVED,
      content::NotificationService::AllSources());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, &gaia_image);
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));

  // Wait for the file to be written to disk then reset the cache.
  save_observer.Wait();
  ResetCache();

  // Try to get the GAIA picture. This should return NULL until the read from
  // disk is done.
  content::WindowedNotificationObserver read_observer(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllSources());
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  read_observer.Wait();
  EXPECT_TRUE(gfx::test::IsEqual(
    gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(0)));
}

TEST_F(ProfileInfoCacheTest, SetSupervisedUserId) {
  GetCache()->AddProfileToCache(
      GetProfilePath("test"), ASCIIToUTF16("Test"),
      base::string16(), 0, std::string());
  EXPECT_FALSE(GetCache()->ProfileIsSupervisedAtIndex(0));

  GetCache()->SetSupervisedUserIdOfProfileAtIndex(0, "TEST_ID");
  EXPECT_TRUE(GetCache()->ProfileIsSupervisedAtIndex(0));
  EXPECT_EQ("TEST_ID", GetCache()->GetSupervisedUserIdOfProfileAtIndex(0));

  ResetCache();
  EXPECT_TRUE(GetCache()->ProfileIsSupervisedAtIndex(0));

  GetCache()->SetSupervisedUserIdOfProfileAtIndex(0, std::string());
  EXPECT_FALSE(GetCache()->ProfileIsSupervisedAtIndex(0));
  EXPECT_EQ("", GetCache()->GetSupervisedUserIdOfProfileAtIndex(0));
}

TEST_F(ProfileInfoCacheTest, EmptyGAIAInfo) {
  base::string16 profile_name = ASCIIToUTF16("name_1");
  int id = profiles::GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(id));

  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), profile_name, base::string16(), 0,
                     std::string());

  // Set empty GAIA info.
  GetCache()->SetGAIANameOfProfileAtIndex(0, base::string16());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, NULL);
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(0, true);

  // Verify that the profile name and picture are not empty.
  EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_TRUE(gfx::test::IsEqual(
      profile_image, GetCache()->GetAvatarIconOfProfileAtIndex(0)));
}

TEST_F(ProfileInfoCacheTest, CreateSupervisedTestingProfile) {
  testing_profile_manager_.CreateTestingProfile("default");
  base::string16 supervised_user_name = ASCIIToUTF16("Supervised User");
  testing_profile_manager_.CreateTestingProfile(
      "test1", scoped_ptr<PrefServiceSyncable>(),
      supervised_user_name, 0, "TEST_ID", TestingProfile::TestingFactories());
  for (size_t i = 0; i < GetCache()->GetNumberOfProfiles(); i++) {
    bool is_supervised =
        GetCache()->GetNameOfProfileAtIndex(i) == supervised_user_name;
    EXPECT_EQ(is_supervised, GetCache()->ProfileIsSupervisedAtIndex(i));
    std::string supervised_user_id = is_supervised ? "TEST_ID" : "";
    EXPECT_EQ(supervised_user_id,
              GetCache()->GetSupervisedUserIdOfProfileAtIndex(i));
  }

  // Supervised profiles have a custom theme, which needs to be deleted on the
  // FILE thread. Reset the profile manager now so everything is deleted while
  // we still have a FILE thread.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
}

TEST_F(ProfileInfoCacheTest, AddStubProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  // Add some profiles with and without a '.' in their paths.
  const struct {
    const char* profile_path;
    const char* profile_name;
  } kTestCases[] = {
    { "path.test0", "name_0" },
    { "path_test1", "name_1" },
    { "path.test2", "name_2" },
    { "path_test3", "name_3" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    base::FilePath profile_path = GetProfilePath(kTestCases[i].profile_path);
    base::string16 profile_name = ASCIIToUTF16(kTestCases[i].profile_name);

    GetCache()->AddProfileToCache(profile_path, profile_name, base::string16(),
                                  i, "");

    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
  }

  ASSERT_EQ(4U, GetCache()->GetNumberOfProfiles());

  // Check that the profiles can be extracted from the local state.
  std::vector<base::string16> names = ProfileInfoCache::GetProfileNames();
  for (size_t i = 0; i < 4; i++)
    ASSERT_FALSE(names[i].empty());
}

// High res avatar downloading is only supported on desktop.
#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_CHROMEOS)
TEST_F(ProfileInfoCacheTest, DownloadHighResAvatarTest) {
  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  EXPECT_EQ(0U, GetCache()->GetNumberOfProfiles());
  base::FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"),
                                base::string16(), 0, std::string());
  EXPECT_EQ(1U, GetCache()->GetNumberOfProfiles());

  // We haven't downloaded any high-res avatars yet.
  EXPECT_EQ(0U, GetCache()->cached_avatar_images_.size());

  // After adding a new profile, the download of high-res avatar will be
  // triggered if the flag kNewAvatarMenu has been set. But the downloader
  // won't ever call OnFetchComplete in the test.
  EXPECT_EQ(1U, GetCache()->avatar_images_downloads_in_progress_.size());

  EXPECT_FALSE(GetCache()->GetHighResAvatarOfProfileAtIndex(0));

  // Simulate downloading a high-res avatar.
  const size_t kIconIndex = 0;
  ProfileAvatarDownloader avatar_downloader(
      kIconIndex, GetCache()->GetPathOfProfileAtIndex(0), GetCache());

  // Put a real bitmap into "bitmap".  2x2 bitmap of green 32 bit pixels.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  bitmap.eraseColor(SK_ColorGREEN);

  avatar_downloader.OnFetchComplete(
      GURL("http://www.google.com/avatar.png"), &bitmap);

  std::string file_name =
      profiles::GetDefaultAvatarIconFileNameAtIndex(kIconIndex);

  // The file should have been cached and saved.
  EXPECT_EQ(1U, GetCache()->avatar_images_downloads_in_progress_.size());
  EXPECT_EQ(1U, GetCache()->cached_avatar_images_.size());
  EXPECT_TRUE(GetCache()->GetHighResAvatarOfProfileAtIndex(0));
  EXPECT_EQ(GetCache()->cached_avatar_images_[file_name],
      GetCache()->GetHighResAvatarOfProfileAtIndex(0));

  // Make sure everything has completed, and the file has been written to disk.
  base::RunLoop().RunUntilIdle();

  // Clean up.
  base::FilePath icon_path =
      profiles::GetPathOfHighResAvatarAtIndex(kIconIndex);
  EXPECT_NE(std::string::npos, icon_path.MaybeAsASCII().find(file_name));
  EXPECT_TRUE(base::PathExists(icon_path));
  EXPECT_TRUE(base::DeleteFile(icon_path, true));
  EXPECT_FALSE(base::PathExists(icon_path));
}
#endif
