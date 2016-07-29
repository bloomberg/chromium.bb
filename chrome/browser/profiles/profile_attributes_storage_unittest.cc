// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_set>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// The ProfileMetadataEntry accessors aren't just plain old accessors to local
// members so they'll be tested. The following helpers will make the testing
// code less verbose.
#define TEST_ACCESSORS(entry_type, entry, member, first_value, second_value) \
    TestAccessors(&entry, \
                  &entry_type::Get ## member, \
                  &entry_type::Set ## member, \
                  first_value, \
                  second_value);

#define TEST_STRING16_ACCESSORS(entry_type, entry, member) \
    TEST_ACCESSORS(entry_type, entry, member, \
        base::ASCIIToUTF16("first_" #member "_value"), \
        base::ASCIIToUTF16("second_" #member "_value"));

#define TEST_STRING_ACCESSORS(entry_type, entry, member) \
    TEST_ACCESSORS(entry_type, entry, member, \
        std::string("first_" #member "_value"), \
        std::string("second_" #member "_value"));

#define TEST_BOOL_ACCESSORS(entry_type, entry, member) \
    TestAccessors(&entry, \
                  &entry_type::member, \
                  &entry_type::Set ## member, \
                  false, \
                  true);

#define TEST_STAT_ACCESSORS(entry_type, entry, member) \
    TestStatAccessors(&entry, \
                      &entry_type::Has ## member, \
                      &entry_type::Get ## member, \
                      &entry_type::Set ## member, \
                      10, \
                      20);

template<typename TValue, typename TGetter, typename TSetter>
void TestAccessors(ProfileAttributesEntry** entry,
    TGetter getter_func,
    TSetter setter_func,
    TValue first_value,
    TValue second_value) {
  (*entry->*setter_func)(first_value);
  EXPECT_EQ(first_value, (*entry->*getter_func)());
  (*entry->*setter_func)(second_value);
  EXPECT_EQ(second_value, (*entry->*getter_func)());
}

template<typename TValue, typename TExist, typename TGetter, typename TSetter>
void TestStatAccessors(ProfileAttributesEntry** entry,
    TExist exist_func,
    TGetter getter_func,
    TSetter setter_func,
    TValue first_value,
    TValue second_value) {
  EXPECT_FALSE((*entry->*exist_func)());
  (*entry->*setter_func)(first_value);
  EXPECT_TRUE((*entry->*exist_func)());
  EXPECT_EQ(first_value, (*entry->*getter_func)());
  (*entry->*setter_func)(second_value);
  EXPECT_TRUE((*entry->*exist_func)());
  EXPECT_EQ(second_value, (*entry->*getter_func)());
}
}  // namespace

class ProfileAttributesStorageTest : public testing::Test {
 public:
  ProfileAttributesStorageTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileAttributesStorageTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  void TearDown() override {
  }

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()->user_data_dir().
        AppendASCII(base_name);
  }

  ProfileAttributesStorage* storage() {
    return profile_info_cache();
  }

  ProfileInfoCache* profile_info_cache() {
    return testing_profile_manager_.profile_info_cache();
  }

  void AddTestingProfile() {
    size_t number_of_profiles = storage()->GetNumberOfProfiles();

    storage()->AddProfile(
        GetProfilePath(base::StringPrintf("testing_profile_path%" PRIuS,
                                          number_of_profiles)),
        base::ASCIIToUTF16(base::StringPrintf("testing_profile_name%" PRIuS,
                                              number_of_profiles)),
        base::StringPrintf("testing_profile_gaia%" PRIuS, number_of_profiles),
        base::ASCIIToUTF16(base::StringPrintf("testing_profile_user%" PRIuS,
                                              number_of_profiles)),
        number_of_profiles,
        std::string(""));

    EXPECT_EQ(number_of_profiles + 1, storage()->GetNumberOfProfiles());
  }

 private:
  TestingProfileManager testing_profile_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ProfileAttributesStorageTest, ProfileNotFound) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry;
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));

  AddTestingProfile();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"), &entry));
}

TEST_F(ProfileAttributesStorageTest, AddProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  storage()->AddProfile(GetProfilePath("new_profile_path_1"),
                        base::ASCIIToUTF16("new_profile_name_1"),
                        std::string("new_profile_gaia_1"),
                        base::ASCIIToUTF16("new_profile_username_1"),
                        1,
                        std::string(""));

  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("new_profile_path_1"), &entry));
  EXPECT_EQ(base::ASCIIToUTF16("new_profile_name_1"), entry->GetName());
}

TEST_F(ProfileAttributesStorageTest, RemoveProfile) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry;
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));

  AddTestingProfile();
  EXPECT_EQ(1U, storage()->GetNumberOfProfiles());

  // Deleting an existing profile should make it un-retrievable.
  storage()->RemoveProfile(GetProfilePath("testing_profile_path0"));
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"), &entry));
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"), &entry));
}

TEST_F(ProfileAttributesStorageTest, MultipleProfiles) {
  EXPECT_EQ(0U, storage()->GetNumberOfProfiles());

  for (size_t i = 0; i < 5; ++i) {
    AddTestingProfile();
    EXPECT_EQ(i + 1, storage()->GetNumberOfProfiles());
    std::vector<ProfileAttributesEntry*> entries =
        storage()->GetAllProfilesAttributes();
    EXPECT_EQ(i + 1, entries.size());
  }

  EXPECT_EQ(5U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));
  EXPECT_EQ(base::ASCIIToUTF16("testing_profile_name0"), entry->GetName());

  storage()->RemoveProfile(GetProfilePath("testing_profile_path0"));
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));
  EXPECT_EQ(4U, storage()->GetNumberOfProfiles());

  std::vector<ProfileAttributesEntry*> entries =
      storage()->GetAllProfilesAttributes();
  for (auto* entry : entries) {
    EXPECT_NE(GetProfilePath("testing_profile_path0"), entry->GetPath());
  }
}

TEST_F(ProfileAttributesStorageTest, InitialValues) {
  AddTestingProfile();

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));
  EXPECT_EQ(GetProfilePath("testing_profile_path0"), entry->GetPath());
  EXPECT_EQ(base::ASCIIToUTF16("testing_profile_name0"), entry->GetName());
  EXPECT_EQ(std::string("testing_profile_gaia0"), entry->GetGAIAId());
  EXPECT_EQ(base::ASCIIToUTF16("testing_profile_user0"), entry->GetUserName());
  EXPECT_EQ(0U, entry->GetAvatarIconIndex());
  EXPECT_EQ(std::string(""), entry->GetSupervisedUserId());
}

TEST_F(ProfileAttributesStorageTest, EntryAccessors) {
  AddTestingProfile();

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));

  EXPECT_EQ(GetProfilePath("testing_profile_path0"), entry->GetPath());

  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, Name);
  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, ShortcutName);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry, LocalAuthCredentials);
  TEST_STRING_ACCESSORS(
      ProfileAttributesEntry, entry, PasswordChangeDetectionToken);
  TEST_ACCESSORS(ProfileAttributesEntry, entry, BackgroundStatus, false, true);
  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, GAIAName);
  TEST_STRING16_ACCESSORS(ProfileAttributesEntry, entry, GAIAGivenName);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingGAIAPicture);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsOmitted);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsSigninRequired);
  TEST_STRING_ACCESSORS(ProfileAttributesEntry, entry, SupervisedUserId);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsEphemeral);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingDefaultName);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsUsingDefaultAvatar);
  TEST_BOOL_ACCESSORS(ProfileAttributesEntry, entry, IsAuthError);

  TEST_STAT_ACCESSORS(ProfileAttributesEntry, entry, StatsBrowsingHistory);
  TEST_STAT_ACCESSORS(ProfileAttributesEntry, entry, StatsBookmarks);
  TEST_STAT_ACCESSORS(ProfileAttributesEntry, entry, StatsPasswords);
  TEST_STAT_ACCESSORS(ProfileAttributesEntry, entry, StatsSettings);
}

TEST_F(ProfileAttributesStorageTest, AuthInfo) {
  AddTestingProfile();

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));

  entry->SetAuthInfo("", base::string16());
  ASSERT_FALSE(entry->IsAuthenticated());
  EXPECT_EQ(base::string16(), entry->GetUserName());
  EXPECT_EQ("", entry->GetGAIAId());

  entry->SetAuthInfo("foo", base::ASCIIToUTF16("bar"));
  ASSERT_TRUE(entry->IsAuthenticated());
  EXPECT_EQ(base::ASCIIToUTF16("bar"), entry->GetUserName());
  EXPECT_EQ("foo", entry->GetGAIAId());
}

TEST_F(ProfileAttributesStorageTest, SupervisedUsersAccessors) {
  AddTestingProfile();

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &entry));

  entry->SetSupervisedUserId("");
  ASSERT_FALSE(entry->IsSupervised());
  ASSERT_FALSE(entry->IsChild());
  ASSERT_FALSE(entry->IsLegacySupervised());

  entry->SetSupervisedUserId("some_supervised_user_id");
  ASSERT_TRUE(entry->IsSupervised());
  ASSERT_FALSE(entry->IsChild());
  ASSERT_TRUE(entry->IsLegacySupervised());

  entry->SetSupervisedUserId(supervised_users::kChildAccountSUID);
  ASSERT_TRUE(entry->IsSupervised());
  ASSERT_TRUE(entry->IsChild());
  ASSERT_FALSE(entry->IsLegacySupervised());
}

TEST_F(ProfileAttributesStorageTest, ReSortTriggered) {
  storage()->AddProfile(GetProfilePath("alpha_path"),
                        base::ASCIIToUTF16("alpha"),
                        std::string("alpha_gaia"),
                        base::ASCIIToUTF16("alpha_username"),
                        1,
                        std::string(""));

  storage()->AddProfile(GetProfilePath("lima_path"),
                        base::ASCIIToUTF16("lima"),
                        std::string("lima_gaia"),
                        base::ASCIIToUTF16("lima_username"),
                        1,
                        std::string(""));

  ProfileAttributesEntry* entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("alpha_path"), &entry));

  // Trigger a ProfileInfoCache re-sort.
  entry->SetName(base::ASCIIToUTF16("zulu_name"));
  EXPECT_EQ(GetProfilePath("alpha_path"), entry->GetPath());
}

TEST_F(ProfileAttributesStorageTest, RemoveOtherProfile) {
  AddTestingProfile();
  AddTestingProfile();
  AddTestingProfile();

  EXPECT_EQ(3U, storage()->GetNumberOfProfiles());

  ProfileAttributesEntry* first_entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &first_entry));

  ProfileAttributesEntry* second_entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"), &second_entry));

  EXPECT_EQ(
      base::ASCIIToUTF16("testing_profile_name0"), first_entry->GetName());

  storage()->RemoveProfile(GetProfilePath("testing_profile_path1"));
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path1"), &second_entry));

  EXPECT_EQ(GetProfilePath("testing_profile_path0"), first_entry->GetPath());
  EXPECT_EQ(
      base::ASCIIToUTF16("testing_profile_name0"), first_entry->GetName());

  // Deleting through the ProfileInfoCache should be reflected in the
  // ProfileAttributesStorage as well.
  profile_info_cache()->RemoveProfile(
      GetProfilePath("testing_profile_path2"));
  ASSERT_FALSE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path2"), &second_entry));
}

TEST_F(ProfileAttributesStorageTest, AccessFromElsewhere) {
  AddTestingProfile();

  ProfileAttributesEntry* first_entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &first_entry));

  ProfileAttributesEntry* second_entry;
  ASSERT_TRUE(storage()->GetProfileAttributesWithPath(
      GetProfilePath("testing_profile_path0"), &second_entry));

  first_entry->SetName(base::ASCIIToUTF16("NewName"));
  EXPECT_EQ(base::ASCIIToUTF16("NewName"), second_entry->GetName());
  EXPECT_EQ(first_entry, second_entry);

  // The ProfileInfoCache should also reflect the changes and its changes
  // should be reflected by the ProfileAttributesStorage.
  size_t index = profile_info_cache()->GetIndexOfProfileWithPath(
      GetProfilePath("testing_profile_path0"));
  EXPECT_EQ(base::ASCIIToUTF16("NewName"),
      profile_info_cache()->GetNameOfProfileAtIndex(index));

  profile_info_cache()->SetNameOfProfileAtIndex(
      index, base::ASCIIToUTF16("OtherNewName"));
  EXPECT_EQ(base::ASCIIToUTF16("OtherNewName"), first_entry->GetName());
}

TEST_F(ProfileAttributesStorageTest, ChooseAvatarIconIndexForNewProfile) {
  size_t total_icon_count = profiles::GetDefaultAvatarIconCount();
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  size_t generic_icon_count = profiles::GetGenericAvatarIconCount();
  ASSERT_LE(generic_icon_count, total_icon_count);
#endif

  // Run ChooseAvatarIconIndexForNewProfile |num_iterations| times before using
  // the final |icon_index| to add a profile. Multiple checks are needed because
  // ChooseAvatarIconIndexForNewProfile is non-deterministic.
  const int num_iterations = 10;
  std::unordered_set<int> used_icon_indices;

  for (size_t i = 0; i < total_icon_count; ++i) {
    EXPECT_EQ(i, storage()->GetNumberOfProfiles());

    size_t icon_index = 0;
    for (int iter = 0; iter < num_iterations; ++iter) {
      icon_index = storage()->ChooseAvatarIconIndexForNewProfile();
      // Icon must not be used.
      ASSERT_EQ(0u, used_icon_indices.count(icon_index));
      ASSERT_GT(total_icon_count, icon_index);

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
      if (i < total_icon_count - generic_icon_count)
        ASSERT_LE(generic_icon_count, icon_index);
      else
        ASSERT_GT(generic_icon_count, icon_index);
#endif
    }

    used_icon_indices.insert(icon_index);

    storage()->AddProfile(
        GetProfilePath(base::StringPrintf("testing_profile_path%" PRIuS, i)),
                       base::string16(),
                       std::string(),
                       base::string16(),
                       icon_index,
                       std::string());
  }

  for (int iter = 0; iter < num_iterations; ++iter) {
    // All icons are used up, expect any valid icon.
    ASSERT_GT(total_icon_count,
              storage()->ChooseAvatarIconIndexForNewProfile());
  }
}
