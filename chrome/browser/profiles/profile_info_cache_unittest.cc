// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_cache_unittest.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using content::BrowserThread;

ProfileNameVerifierObserver::ProfileNameVerifierObserver(
    TestingProfileManager* testing_profile_manager)
    : testing_profile_manager_(testing_profile_manager) {
  DCHECK(testing_profile_manager_);
}

ProfileNameVerifierObserver::~ProfileNameVerifierObserver() {
}

void ProfileNameVerifierObserver::OnProfileAdded(
    const FilePath& profile_path) {
  string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) == profile_names_.end());
  profile_names_.insert(profile_name);
}

void ProfileNameVerifierObserver::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
  string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) != profile_names_.end());
  profile_names_.erase(profile_name);
}

void ProfileNameVerifierObserver::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
  EXPECT_TRUE(profile_names_.find(profile_name) == profile_names_.end());
}

void ProfileNameVerifierObserver::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  string16 new_profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(old_profile_name) != profile_names_.end());
  EXPECT_TRUE(profile_names_.find(new_profile_name) == profile_names_.end());
  profile_names_.erase(old_profile_name);
  profile_names_.insert(new_profile_name);
}

void ProfileNameVerifierObserver::OnProfileAvatarChanged(
    const FilePath& profile_path) {
  string16 profile_name = GetCache()->GetNameOfProfileAtIndex(
      GetCache()->GetIndexOfProfileWithPath(profile_path));
  EXPECT_TRUE(profile_names_.find(profile_name) != profile_names_.end());
}

ProfileInfoCache* ProfileNameVerifierObserver::GetCache() {
  return testing_profile_manager_->profile_info_cache();
}

ProfileInfoCacheTest::ProfileInfoCacheTest()
    : testing_profile_manager_(
        TestingBrowserProcess::GetGlobal()),
      ui_thread_(BrowserThread::UI, &ui_loop_),
      file_thread_(BrowserThread::FILE, &ui_loop_),
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
  ui_loop_.RunUntilIdle();
}

ProfileInfoCache* ProfileInfoCacheTest::GetCache() {
  return testing_profile_manager_.profile_info_cache();
}

FilePath ProfileInfoCacheTest::GetProfilePath(
    const std::string& base_name) {
  return testing_profile_manager_.profile_manager()->user_data_dir().
      AppendASCII(base_name);
}

void ProfileInfoCacheTest::ResetCache() {
  testing_profile_manager_.DeleteProfileInfoCache();
}

namespace {

TEST_F(ProfileInfoCacheTest, AddProfiles) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (uint32 i = 0; i < 4; ++i) {
    FilePath profile_path = GetProfilePath(StringPrintf("path_%ud", i));
    string16 profile_name = ASCIIToUTF16(StringPrintf("name_%ud", i));
    const SkBitmap* icon = rb.GetImageNamed(
        ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(
            i)).ToSkBitmap();

    GetCache()->AddProfileToCache(profile_path, profile_name, string16(), i,
                                  false);
    GetCache()->SetBackgroundStatusOfProfileAtIndex(i, true);
    string16 gaia_name = ASCIIToUTF16(StringPrintf("gaia_%ud", i));
    GetCache()->SetGAIANameOfProfileAtIndex(i, gaia_name);

    EXPECT_EQ(i + 1, GetCache()->GetNumberOfProfiles());
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(profile_path, GetCache()->GetPathOfProfileAtIndex(i));
    const SkBitmap* actual_icon = GetCache()->GetAvatarIconOfProfileAtIndex(
        i).ToSkBitmap();
    EXPECT_EQ(icon->width(), actual_icon->width());
    EXPECT_EQ(icon->height(), actual_icon->height());
  }

  // Reset the cache and test the it reloads correctly.
  ResetCache();

  EXPECT_EQ(4u, GetCache()->GetNumberOfProfiles());
  for (uint32 i = 0; i < 4; ++i) {
    FilePath profile_path = GetProfilePath(StringPrintf("path_%ud", i));
    EXPECT_EQ(i, GetCache()->GetIndexOfProfileWithPath(profile_path));
    string16 profile_name = ASCIIToUTF16(StringPrintf("name_%ud", i));
    EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(i));
    EXPECT_EQ(i, GetCache()->GetAvatarIconIndexOfProfileAtIndex(i));
    EXPECT_EQ(true, GetCache()->GetBackgroundStatusOfProfileAtIndex(i));
    string16 gaia_name = ASCIIToUTF16(StringPrintf("gaia_%ud", i));
    EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(i));
  }
}

TEST_F(ProfileInfoCacheTest, DeleteProfile) {
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());

  FilePath path_1 = GetProfilePath("path_1");
  GetCache()->AddProfileToCache(path_1, ASCIIToUTF16("name_1"), string16(),
                                0, false);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());

  FilePath path_2 = GetProfilePath("path_2");
  string16 name_2 = ASCIIToUTF16("name_2");
  GetCache()->AddProfileToCache(path_2, name_2, string16(), 0, false);
  EXPECT_EQ(2u, GetCache()->GetNumberOfProfiles());

  GetCache()->DeleteProfileFromCache(path_1);
  EXPECT_EQ(1u, GetCache()->GetNumberOfProfiles());
  EXPECT_EQ(name_2, GetCache()->GetNameOfProfileAtIndex(0));

  GetCache()->DeleteProfileFromCache(path_2);
  EXPECT_EQ(0u, GetCache()->GetNumberOfProfiles());
}

TEST_F(ProfileInfoCacheTest, MutateProfile) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"), string16(), 0, false);

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

TEST_F(ProfileInfoCacheTest, Sort) {
  string16 name_a = ASCIIToUTF16("apple");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_a"), name_a, string16(), 0, false);

  string16 name_c = ASCIIToUTF16("cat");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_c"), name_c, string16(), 0, false);

  // Sanity check the initial order.
  EXPECT_EQ(name_a, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_EQ(name_c, GetCache()->GetNameOfProfileAtIndex(1));

  // Add a new profile (start with a capital to test case insensitive sorting.
  string16 name_b = ASCIIToUTF16("Banana");
  GetCache()->AddProfileToCache(
      GetProfilePath("path_b"), name_b, string16(), 0, false);

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
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"), string16(), 0, false);

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

TEST_F(ProfileInfoCacheTest, HasMigrated) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"), string16(), 0, false);

  // Sanity check.
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(1));

  // Set migrated state for 2nd profile.
  GetCache()->SetHasMigratedToGAIAInfoOfProfileAtIndex(1, true);
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(0));
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(1));

  // Set migrated state for 1st profile.
  GetCache()->SetHasMigratedToGAIAInfoOfProfileAtIndex(0, true);
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(0));
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(1));

  // Unset migrated state for 2nd profile.
  GetCache()->SetHasMigratedToGAIAInfoOfProfileAtIndex(1, false);
  EXPECT_TRUE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->GetHasMigratedToGAIAInfoOfProfileAtIndex(1));
}

TEST_F(ProfileInfoCacheTest, GAIAName) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
  string16 profile_name(ASCIIToUTF16("profile name 2"));
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), profile_name, string16(), 0, false);

  // Sanity check.
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(0).empty());
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(1).empty());
  EXPECT_FALSE(GetCache()->IsUsingGAIANameOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->IsUsingGAIANameOfProfileAtIndex(1));

  // Set GAIA name.
  string16 gaia_name(ASCIIToUTF16("Pat Smith"));
  GetCache()->SetGAIANameOfProfileAtIndex(1, gaia_name);
  EXPECT_TRUE(GetCache()->GetGAIANameOfProfileAtIndex(0).empty());
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(1));
  EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(1));

  // Use GAIA name as profile name.
  GetCache()->SetIsUsingGAIANameOfProfileAtIndex(1, true);

  EXPECT_EQ(gaia_name, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(1));

  // Don't use GAIA name as profile name.
  GetCache()->SetIsUsingGAIANameOfProfileAtIndex(1, false);
  EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(1));
  EXPECT_EQ(gaia_name, GetCache()->GetGAIANameOfProfileAtIndex(1));
}

TEST_F(ProfileInfoCacheTest, GAIAPicture) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
  GetCache()->AddProfileToCache(
      GetProfilePath("path_2"), ASCIIToUTF16("name_2"), string16(), 0, false);

  // Sanity check.
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(1));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(0));
  EXPECT_FALSE(GetCache()->IsUsingGAIAPictureOfProfileAtIndex(1));

  // The profile icon should be the default one.
  int id = ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(id));
  EXPECT_TRUE(gfx::test::IsEqual(
      profile_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Set GAIA picture.
  gfx::Image gaia_image(gfx::test::CreateImage());
  GetCache()->SetGAIAPictureOfProfileAtIndex(1, &gaia_image);
  EXPECT_EQ(NULL, GetCache()->GetGAIAPictureOfProfileAtIndex(0));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::IsEqual(
      profile_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Use GAIA picture as profile picture.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, true);
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));

  // Don't use GAIA picture as profile picture.
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(1, false);
  EXPECT_TRUE(gfx::test::IsEqual(
      gaia_image, *GetCache()->GetGAIAPictureOfProfileAtIndex(1)));
  EXPECT_TRUE(gfx::test::IsEqual(
      profile_image, GetCache()->GetAvatarIconOfProfileAtIndex(1)));
}

TEST_F(ProfileInfoCacheTest, PersistGAIAPicture) {
  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), ASCIIToUTF16("name_1"), string16(), 0, false);
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

TEST_F(ProfileInfoCacheTest, EmptyGAIAInfo) {
  string16 profile_name = ASCIIToUTF16("name_1");
  int id = ProfileInfoCache::GetDefaultAvatarIconResourceIDAtIndex(0);
  const gfx::Image& profile_image(
      ResourceBundle::GetSharedInstance().GetImageNamed(id));

  GetCache()->AddProfileToCache(
      GetProfilePath("path_1"), profile_name, string16(), 0, false);

  // Set empty GAIA info.
  GetCache()->SetGAIANameOfProfileAtIndex(0, string16());
  GetCache()->SetGAIAPictureOfProfileAtIndex(0, NULL);
  GetCache()->SetIsUsingGAIANameOfProfileAtIndex(0, true);
  GetCache()->SetIsUsingGAIAPictureOfProfileAtIndex(0, true);

  // Verify that the profile name and picture are not empty.
  EXPECT_EQ(profile_name, GetCache()->GetNameOfProfileAtIndex(0));
  EXPECT_TRUE(gfx::test::IsEqual(
      profile_image, GetCache()->GetAvatarIconOfProfileAtIndex(0)));
}

}  // namespace
