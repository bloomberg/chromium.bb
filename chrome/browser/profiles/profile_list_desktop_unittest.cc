// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_list_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using base::ASCIIToUTF16;

namespace {

class MockObserver : public AvatarMenuObserver {
 public:
  MockObserver() : count_(0) {}
  virtual ~MockObserver() {}

  virtual void OnAvatarMenuChanged(
      AvatarMenu* avatar_menu) OVERRIDE {
    ++count_;
  }

  int change_count() const { return count_; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class ProfileListDesktopTest : public testing::Test {
 public:
  ProfileListDesktopTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(manager_.SetUp());
#if defined(OS_CHROMEOS)
    // AvatarMenu and multiple profiles works after user logged in.
    manager_.SetLoggedIn(true);
#endif
  }

  AvatarMenu* GetAvatarMenu() {
    // Reset the MockObserver.
    mock_observer_.reset(new MockObserver());
    EXPECT_EQ(0, change_count());

    // Reset the model.
    avatar_menu_.reset(new AvatarMenu(
        manager()->profile_info_cache(),
        mock_observer_.get(),
        NULL));
    avatar_menu_->RebuildMenu();
    EXPECT_EQ(0, change_count());
    return avatar_menu_.get();
  }

  TestingProfileManager* manager() { return &manager_; }

  void AddOmittedProfile(std::string name) {
    ProfileInfoCache* cache = manager()->profile_info_cache();
    cache->AddProfileToCache(
        cache->GetUserDataDir().AppendASCII(name), ASCIIToUTF16(name),
        base::string16(), 0, "TEST_ID");
  }

  int change_count() const { return mock_observer_->change_count(); }

 private:
  TestingProfileManager manager_;
  scoped_ptr<MockObserver> mock_observer_;
  scoped_ptr<AvatarMenu> avatar_menu_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListDesktopTest);
};

TEST_F(ProfileListDesktopTest, InitialCreation) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* model = GetAvatarMenu();
  EXPECT_EQ(0, change_count());

  ASSERT_EQ(2U, model->GetNumberOfItems());

  const AvatarMenu::Item& item1 = model->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenu::Item& item2 = model->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item2.name);
}

TEST_F(ProfileListDesktopTest, NoOmittedProfiles) {
  ProfileListDesktop profile_list(manager()->profile_info_cache());

  // Profiles are stored and listed alphabetically.
  manager()->CreateTestingProfile("1 included");
  manager()->CreateTestingProfile("2 included");
  manager()->CreateTestingProfile("3 included");
  manager()->CreateTestingProfile("4 included");

  profile_list.RebuildMenu();
  ASSERT_EQ(4u, profile_list.GetNumberOfItems());

  const AvatarMenu::Item& item1 = profile_list.GetItemAt(0);
  EXPECT_EQ(0u, item1.menu_index);
  EXPECT_EQ(0u, item1.profile_index);
  EXPECT_EQ(ASCIIToUTF16("1 included"), item1.name);

  const AvatarMenu::Item& item2 = profile_list.GetItemAt(1);
  EXPECT_EQ(1u, item2.menu_index);
  EXPECT_EQ(1u, item2.profile_index);
  EXPECT_EQ(ASCIIToUTF16("2 included"), item2.name);

  const AvatarMenu::Item& item3 = profile_list.GetItemAt(2);
  EXPECT_EQ(2u, item3.menu_index);
  EXPECT_EQ(2u, item3.profile_index);
  EXPECT_EQ(ASCIIToUTF16("3 included"), item3.name);

  const AvatarMenu::Item& item4 = profile_list.GetItemAt(3);
  EXPECT_EQ(3u, item4.menu_index);
  EXPECT_EQ(3u, item4.profile_index);
  EXPECT_EQ(ASCIIToUTF16("4 included"), item4.name);

  EXPECT_EQ(0u, profile_list.MenuIndexFromProfileIndex(0));
  EXPECT_EQ(1u, profile_list.MenuIndexFromProfileIndex(1));
  EXPECT_EQ(2u, profile_list.MenuIndexFromProfileIndex(2));
  EXPECT_EQ(3u, profile_list.MenuIndexFromProfileIndex(3));
}

TEST_F(ProfileListDesktopTest, WithOmittedProfiles) {
  ProfileListDesktop profile_list(manager()->profile_info_cache());

  // Profiles are stored and listed alphabetically.
  AddOmittedProfile("0 omitted");
  manager()->CreateTestingProfile("1 included");
  AddOmittedProfile("2 omitted");
  manager()->CreateTestingProfile("3 included");
  manager()->CreateTestingProfile("4 included");
  AddOmittedProfile("5 omitted");
  manager()->CreateTestingProfile("6 included");
  AddOmittedProfile("7 omitted");

  profile_list.RebuildMenu();
  ASSERT_EQ(4u, profile_list.GetNumberOfItems());

  const AvatarMenu::Item& item1 = profile_list.GetItemAt(0);
  EXPECT_EQ(0u, item1.menu_index);
  EXPECT_EQ(1u, item1.profile_index);
  EXPECT_EQ(ASCIIToUTF16("1 included"), item1.name);

  const AvatarMenu::Item& item2 = profile_list.GetItemAt(1);
  EXPECT_EQ(1u, item2.menu_index);
  EXPECT_EQ(3u, item2.profile_index);
  EXPECT_EQ(ASCIIToUTF16("3 included"), item2.name);

  const AvatarMenu::Item& item3 = profile_list.GetItemAt(2);
  EXPECT_EQ(2u, item3.menu_index);
  EXPECT_EQ(4u, item3.profile_index);
  EXPECT_EQ(ASCIIToUTF16("4 included"), item3.name);

  const AvatarMenu::Item& item4 = profile_list.GetItemAt(3);
  EXPECT_EQ(3u, item4.menu_index);
  EXPECT_EQ(6u, item4.profile_index);
  EXPECT_EQ(ASCIIToUTF16("6 included"), item4.name);

  EXPECT_EQ(0u, profile_list.MenuIndexFromProfileIndex(1));
  EXPECT_EQ(1u, profile_list.MenuIndexFromProfileIndex(3));
  EXPECT_EQ(2u, profile_list.MenuIndexFromProfileIndex(4));
  EXPECT_EQ(3u, profile_list.MenuIndexFromProfileIndex(6));
}

TEST_F(ProfileListDesktopTest, ActiveItem) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* model = GetAvatarMenu();
  ASSERT_EQ(2U, model->GetNumberOfItems());
  // TODO(jeremy): Expand test to verify active profile index other than 0
  // crbug.com/100871
  ASSERT_EQ(0U, model->GetActiveProfileIndex());
}

TEST_F(ProfileListDesktopTest, ModifyingNameResortsCorrectly) {
  std::string name1("Alpha");
  std::string name2("Beta");
  std::string newname1("Gamma");

  manager()->CreateTestingProfile(name1);
  manager()->CreateTestingProfile(name2);

  AvatarMenu* model = GetAvatarMenu();
  EXPECT_EQ(0, change_count());

  ASSERT_EQ(2U, model->GetNumberOfItems());

  const AvatarMenu::Item& item1 = model->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name1), item1.name);

  const AvatarMenu::Item& item2 = model->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name2), item2.name);

  // Change name of the first profile, to trigger resorting of the profiles:
  // now the first model should be named "beta", and the second be "gamma".
  manager()->profile_info_cache()->SetNameOfProfileAtIndex(0,
        ASCIIToUTF16(newname1));
  const AvatarMenu::Item& item1next = model->GetItemAt(0);
  EXPECT_GT(change_count(), 1);
  EXPECT_EQ(0U, item1next.menu_index);
  EXPECT_EQ(ASCIIToUTF16(name2), item1next.name);

  const AvatarMenu::Item& item2next = model->GetItemAt(1);
  EXPECT_EQ(1U, item2next.menu_index);
  EXPECT_EQ(ASCIIToUTF16(newname1), item2next.name);
}

TEST_F(ProfileListDesktopTest, ChangeOnNotify) {
  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

  AvatarMenu* model = GetAvatarMenu();
  EXPECT_EQ(0, change_count());
  EXPECT_EQ(2U, model->GetNumberOfItems());

  manager()->CreateTestingProfile("Test 3");

  // Four changes happened via the call to CreateTestingProfile: adding the
  // profile to the cache, setting the user name, rebuilding the list of
  // profiles after the name change, and changing the avatar.
  // On Windows, an extra change happens to set the shortcut name for the
  // profile.
  // TODO(michaelpg): Determine why five changes happen on ChromeOS and
  // investigate other platforms.
  EXPECT_GE(change_count(), 4);
  ASSERT_EQ(3U, model->GetNumberOfItems());

  const AvatarMenu::Item& item1 = model->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenu::Item& item2 = model->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item2.name);

  const AvatarMenu::Item& item3 = model->GetItemAt(2);
  EXPECT_EQ(2U, item3.menu_index);
  EXPECT_EQ(ASCIIToUTF16("Test 3"), item3.name);
}

TEST_F(ProfileListDesktopTest, ShowAvatarMenuInTrial) {
  // If multiprofile mode is not enabled, the trial will not be enabled, so it
  // isn't tested.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  base::FieldTrialList field_trial_list_(NULL);
  base::FieldTrialList::CreateFieldTrial("ShowProfileSwitcher", "AlwaysShow");

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());
#else
  EXPECT_TRUE(AvatarMenu::ShouldShowAvatarMenu());
#endif
}

TEST_F(ProfileListDesktopTest, DontShowOldAvatarMenuForSingleProfile) {
  switches::DisableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  manager()->CreateTestingProfile("Test 1");

  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());

  // If multiprofile mode is enabled, there are no other cases when we wouldn't
  // show the menu.
  if (profiles::IsMultipleProfilesEnabled())
    return;

  manager()->CreateTestingProfile("Test 2");

  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());
}

TEST_F(ProfileListDesktopTest, AlwaysShowNewAvatarMenu) {
  // If multiprofile mode is not enabled then the menu is never shown.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  switches::EnableNewAvatarMenuForTesting(CommandLine::ForCurrentProcess());

  manager()->CreateTestingProfile("Test 1");

  EXPECT_TRUE(AvatarMenu::ShouldShowAvatarMenu());
}

TEST_F(ProfileListDesktopTest, ShowAvatarMenu) {
  // If multiprofile mode is not enabled then the menu is never shown.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  manager()->CreateTestingProfile("Test 1");
  manager()->CreateTestingProfile("Test 2");

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());
#else
  EXPECT_TRUE(AvatarMenu::ShouldShowAvatarMenu());
#endif
}

TEST_F(ProfileListDesktopTest, SyncState) {
  // If multiprofile mode is not enabled then the menu is never shown.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  manager()->CreateTestingProfile("Test 1");

  // Add a managed user profile.
  ProfileInfoCache* cache = manager()->profile_info_cache();
  base::FilePath path = cache->GetUserDataDir().AppendASCII("p2");
  cache->AddProfileToCache(path, ASCIIToUTF16("Test 2"), base::string16(), 0,
                           "TEST_ID");
  cache->SetIsOmittedProfileAtIndex(cache->GetIndexOfProfileWithPath(path),
                                    false);

  AvatarMenu* model = GetAvatarMenu();
  model->RebuildMenu();
  EXPECT_EQ(2U, model->GetNumberOfItems());

  // Now check that the sync_state of a supervised user shows the supervised
  // user avatar label instead.
  base::string16 supervised_user_label =
      l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_AVATAR_LABEL);
  const AvatarMenu::Item& item1 = model->GetItemAt(0);
  EXPECT_NE(item1.sync_state, supervised_user_label);

  const AvatarMenu::Item& item2 = model->GetItemAt(1);
  EXPECT_EQ(item2.sync_state, supervised_user_label);
}

}  // namespace
