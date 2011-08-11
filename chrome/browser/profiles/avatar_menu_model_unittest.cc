// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu_model.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/profiles/profile_info_interface.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_browser_process_test.h"
#include "content/common/notification_service.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class FakeProfileInfo : public ProfileInfoInterface {
 public:
  FakeProfileInfo() {}
  virtual ~FakeProfileInfo() {}

  std::vector<AvatarMenuModel::Item*>* mock_profiles() {
   return &profiles_;
  }

  virtual size_t GetNumberOfProfiles() const OVERRIDE {
    return profiles_.size();
  }

  virtual size_t GetIndexOfProfileWithPath(
      const FilePath& profile_path) const OVERRIDE {
    return std::string::npos;
  }

  virtual string16 GetNameOfProfileAtIndex(size_t index) const OVERRIDE {
    return profiles_[index]->name;
  }

  virtual FilePath GetPathOfProfileAtIndex(size_t index) const OVERRIDE {
    return FilePath();
  }

  virtual const gfx::Image& GetAvatarIconOfProfileAtIndex(
      size_t index) const OVERRIDE {
    return profiles_[index]->icon;
  }

 private:
  std::vector<AvatarMenuModel::Item*> profiles_;
};

class MockObserver : public AvatarMenuModelObserver {
 public:
  MockObserver() : count_(0) {}
  virtual ~MockObserver() {}

  virtual void OnAvatarMenuModelChanged(AvatarMenuModel* avatar_menu_model) {
    ++count_;
  }

  int change_count() { return count_; }

 private:
  int count_;
};

class AvatarMenuModelTest : public TestingBrowserProcessTest {
 public:
  FakeProfileInfo* cache() {
    return &cache_;
  }

  Browser* browser() {
    return NULL;
  }

  const gfx::Image& GetTestImage() {
    return ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_PROFILE_AVATAR_0);
  }

 private:
  FakeProfileInfo cache_;
};

TEST_F(AvatarMenuModelTest, InitialCreation) {
  std::vector<AvatarMenuModel::Item*>* profiles = cache()->mock_profiles();

  AvatarMenuModel::Item profile1(0, GetTestImage());
  profile1.name = ASCIIToUTF16("Test 1");
  profiles->push_back(&profile1);

  AvatarMenuModel::Item profile2(1, GetTestImage());
  profile2.name = ASCIIToUTF16("Test 2");
  profiles->push_back(&profile2);

  MockObserver observer;
  EXPECT_EQ(0, observer.change_count());

  AvatarMenuModel model(cache(), &observer, browser());
  EXPECT_EQ(1, observer.change_count());

  ASSERT_EQ(2U, model.GetNumberOfItems());

  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_EQ(0U, item1.model_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(1U, item2.model_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item2.name);
}


TEST_F(AvatarMenuModelTest, ChangeOnNotify) {
  std::vector<AvatarMenuModel::Item*>* profiles = cache()->mock_profiles();

  AvatarMenuModel::Item profile1(0, GetTestImage());
  profile1.name = ASCIIToUTF16("Test 1");
  profiles->push_back(&profile1);

  AvatarMenuModel::Item profile2(1, GetTestImage());
  profile2.name = ASCIIToUTF16("Test 2");
  profiles->push_back(&profile2);

  MockObserver observer;
  EXPECT_EQ(0, observer.change_count());

  AvatarMenuModel model(cache(), &observer, browser());
  EXPECT_EQ(1, observer.change_count());
  EXPECT_EQ(2U, model.GetNumberOfItems());

  AvatarMenuModel::Item profile3(2, GetTestImage());
  profile3.name = ASCIIToUTF16("Test 3");
  profiles->insert(profiles->begin() + 1, &profile3);

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());
  EXPECT_EQ(2, observer.change_count());
  ASSERT_EQ(3U, model.GetNumberOfItems());

  const AvatarMenuModel::Item& item1 = model.GetItemAt(0);
  EXPECT_EQ(0U, item1.model_index);
  EXPECT_EQ(ASCIIToUTF16("Test 1"), item1.name);

  const AvatarMenuModel::Item& item2 = model.GetItemAt(1);
  EXPECT_EQ(1U, item2.model_index);
  EXPECT_EQ(ASCIIToUTF16("Test 3"), item2.name);

  const AvatarMenuModel::Item& item3 = model.GetItemAt(2);
  EXPECT_EQ(2U, item3.model_index);
  EXPECT_EQ(ASCIIToUTF16("Test 2"), item3.name);
}

}  // namespace
