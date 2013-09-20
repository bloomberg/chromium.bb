// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class MockObserver : public AvatarMenuModelObserver {
 public:
  MockObserver() : count_(0) {}
  virtual ~MockObserver() {}

  virtual void OnAvatarMenuModelChanged(
      AvatarMenuModel* avatar_menu_model) OVERRIDE{
    ++count_;
  }

  int change_count() { return count_; }

 private:
  int count_;
};

class AvatarMenuModelTest : public testing::Test {
 public:
  AvatarMenuModelTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(manager_.SetUp());
#if defined(OS_CHROMEOS)
    // AvatarMenu and multiple profiles works after user logged in.
    manager_.SetLoggedIn(true);
#endif
  }

  Browser* browser() { return NULL; }

  TestingProfileManager* manager() { return &manager_; }

 private:
  TestingProfileManager manager_;
};

TEST_F(AvatarMenuModelTest, InitialCreation) {
  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

  MockObserver observer;
  EXPECT_EQ(0, observer.change_count());

  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  EXPECT_EQ(0, observer.change_count());

  ASSERT_EQ(2U, model.GetNumberOfItems());

  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_EQ(0U, item1.model_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(1U, item2.model_index);
  EXPECT_EQ(name2, item2.name);
}

TEST_F(AvatarMenuModelTest, ActiveItem) {
  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

  MockObserver observer;
  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  ASSERT_EQ(2U, model.GetNumberOfItems());
  // TODO(jeremy): Expand test to verify active profile index other than 0
  // crbug.com/100871
  ASSERT_EQ(0U, model.GetActiveProfileIndex());
}

TEST_F(AvatarMenuModelTest, ModifyingNameResortsCorrectly) {
  string16 name1(ASCIIToUTF16("Alpha"));
  string16 name2(ASCIIToUTF16("Beta"));
  string16 newname1(ASCIIToUTF16("Gamma"));

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

  MockObserver observer;
  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  EXPECT_EQ(0, observer.change_count());

  ASSERT_EQ(2U, model.GetNumberOfItems());

  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_EQ(0U, item1.model_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(1U, item2.model_index);
  EXPECT_EQ(name2, item2.name);

  // Change name of the first profile, to trigger resorting of the profiles:
  // now the first model should be named "beta", and the second be "gamma".
  manager()->profile_info_cache()->SetNameOfProfileAtIndex(0, newname1);
  const AvatarMenuModel::Item& item1next = model.GetItemAt(0);
  EXPECT_GT(observer.change_count(), 1);
  EXPECT_EQ(0U, item1next.model_index);
  EXPECT_EQ(name2, item1next.name);

  const AvatarMenuModel::Item& item2next = model.GetItemAt(1);
  EXPECT_EQ(1U, item2next.model_index);
  EXPECT_EQ(newname1, item2next.name);
}

TEST_F(AvatarMenuModelTest, ChangeOnNotify) {
  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

  MockObserver observer;
  EXPECT_EQ(0, observer.change_count());

  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  EXPECT_EQ(0, observer.change_count());
  EXPECT_EQ(2U, model.GetNumberOfItems());

  string16 name3(ASCIIToUTF16("Test 3"));
  manager()->CreateTestingProfile("p3", scoped_ptr<PrefServiceSyncable>(),
                                  name3, 0, std::string());

  // Four changes happened via the call to CreateTestingProfile: adding the
  // profile to the cache, setting the user name, rebuilding the list of
  // profiles after the name change, and changing the avatar.
  // On Windows, an extra change happens to set the shortcut name for the
  // profile.
  EXPECT_GE(observer.change_count(), 4);
  ASSERT_EQ(3U, model.GetNumberOfItems());

  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_EQ(0U, item1.model_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(1U, item2.model_index);
  EXPECT_EQ(name2, item2.name);

  const AvatarMenuModel::Item& item3 = model.GetItemAt(2);
  EXPECT_EQ(2U, item3.model_index);
  EXPECT_EQ(name3, item3.name);
}

TEST_F(AvatarMenuModelTest, ShowAvatarMenuInTrial) {
  // If multiprofile mode is not enabled, the trial will not be enabled, so it
  // isn't tested.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  base::FieldTrialList field_trial_list_(NULL);
  base::FieldTrialList::CreateFieldTrial("ShowProfileSwitcher", "AlwaysShow");

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(AvatarMenuModel::ShouldShowAvatarMenu());
#else
  EXPECT_TRUE(AvatarMenuModel::ShouldShowAvatarMenu());
#endif
}

TEST_F(AvatarMenuModelTest, DontShowAvatarMenu) {
  string16 name1(ASCIIToUTF16("Test 1"));
  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());

  EXPECT_FALSE(AvatarMenuModel::ShouldShowAvatarMenu());

  // If multiprofile mode is enabled, there are no other cases when we wouldn't
  // show the menu.
  if (profiles::IsMultipleProfilesEnabled())
    return;

  string16 name2(ASCIIToUTF16("Test 2"));
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

  EXPECT_FALSE(AvatarMenuModel::ShouldShowAvatarMenu());
}

TEST_F(AvatarMenuModelTest, ShowAvatarMenu) {
  // If multiprofile mode is not enabled then the menu is never shown.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  name1, 0, std::string());
  manager()->CreateTestingProfile("p2", scoped_ptr<PrefServiceSyncable>(),
                                  name2, 0, std::string());

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(AvatarMenuModel::ShouldShowAvatarMenu());
#else
  EXPECT_TRUE(AvatarMenuModel::ShouldShowAvatarMenu());
#endif
}

TEST_F(AvatarMenuModelTest, SyncState) {
  // If multiprofile mode is not enabled then the menu is never shown.
  if (!profiles::IsMultipleProfilesEnabled())
    return;

  manager()->CreateTestingProfile("p1", scoped_ptr<PrefServiceSyncable>(),
                                  ASCIIToUTF16("Test 1"), 0, std::string());

  // Add a managed user profile.
  ProfileInfoCache* cache = manager()->profile_info_cache();
  manager()->profile_info_cache()->AddProfileToCache(
      cache->GetUserDataDir().AppendASCII("p2"), ASCIIToUTF16("Test 2"),
      string16(), 0, "TEST_ID");
  MockObserver observer;
  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  EXPECT_EQ(2U, model.GetNumberOfItems());

  // Now check that the sync_state of a managed user shows the managed user
  // avatar label instead.
  base::string16 managed_user_label =
      l10n_util::GetStringUTF16(IDS_MANAGED_USER_AVATAR_LABEL);
  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_NE(item1.sync_state, managed_user_label);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(item2.sync_state, managed_user_label);
}

}  // namespace
