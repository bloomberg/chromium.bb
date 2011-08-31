// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

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
      : manager_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(manager_.SetUp());
  }

  Browser* browser() { return NULL; }

  TestingProfileManager* manager() { return &manager_; }

 private:
  TestingProfileManager manager_;
};

TEST_F(AvatarMenuModelTest, InitialCreation) {
  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", name1, 0);
  manager()->CreateTestingProfile("p2", name2, 0);

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


TEST_F(AvatarMenuModelTest, ChangeOnNotify) {
  string16 name1(ASCIIToUTF16("Test 1"));
  string16 name2(ASCIIToUTF16("Test 2"));

  manager()->CreateTestingProfile("p1", name1, 0);
  manager()->CreateTestingProfile("p2", name2, 0);

  MockObserver observer;
  EXPECT_EQ(0, observer.change_count());

  AvatarMenuModel model(manager()->profile_info_cache(), &observer, browser());
  EXPECT_EQ(0, observer.change_count());
  EXPECT_EQ(2U, model.GetNumberOfItems());

  string16 name3(ASCIIToUTF16("Test 3"));
  manager()->CreateTestingProfile("p3", name3, 0);

  // Three changes happened via the call to CreateTestingProfile: adding the
  // profile to the cache, setting the user name, and changing the avatar.
  EXPECT_EQ(3, observer.change_count());
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

}  // namespace
