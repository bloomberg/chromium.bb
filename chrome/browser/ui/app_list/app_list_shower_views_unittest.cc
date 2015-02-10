// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_shower_views.h"

#include "base/files/file_path.h"
#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/test/fake_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAppListShower : public AppListShower {
 public:
  explicit FakeAppListShower(AppListShowerDelegate* delegate)
      : AppListShower(delegate), has_view_(false), visible_(false) {}

  void ShowForProfile(Profile* requested_profile) {
    CreateViewForProfile(requested_profile);
    ShowForCurrentProfile();
  }

  // AppListShower:
  void HandleViewBeingDestroyed() override {
    AppListShower::HandleViewBeingDestroyed();
    has_view_ = false;
    visible_ = false;
  }

  bool IsAppListVisible() const override { return visible_; }

  app_list::AppListView* MakeViewForCurrentProfile() override {
    has_view_ = true;
    return NULL;
  }

  void UpdateViewForNewProfile() override {}

  void Show() override { visible_ = true; }

  void Hide() override { visible_ = false; }

  bool HasView() const override { return has_view_; }

 private:
  bool has_view_;
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppListShower);
};

}  // namespace

class AppListShowerUnitTest : public testing::Test,
                              public AppListShowerDelegate {
 public:
  AppListShowerUnitTest()
      : views_created_(0),
        views_dismissed_(0) {}

  void SetUp() override {
    shower_.reset(new FakeAppListShower(this));
    profile1_ = CreateProfile("p1").Pass();
    profile2_ = CreateProfile("p2").Pass();
  }

  void TearDown() override {}

  scoped_ptr<FakeProfile> CreateProfile(const std::string& name) {
    return make_scoped_ptr(new FakeProfile(name));
  }

  // AppListShowerDelegate:
  AppListViewDelegate* GetViewDelegateForCreate() override { return NULL; }

  bool HasKeepAlive() const {
    return shower_->keep_alive_.get() != NULL;
  }

  void OnViewCreated() override { ++views_created_; }
  void OnViewDismissed() override { ++views_dismissed_; }
  void MoveNearCursor(app_list::AppListView* view) override {}

 protected:
  scoped_ptr<FakeAppListShower> shower_;
  scoped_ptr<FakeProfile> profile1_;
  scoped_ptr<FakeProfile> profile2_;

  int views_created_;
  int views_dismissed_;
};

TEST_F(AppListShowerUnitTest, Preconditions) {
  EXPECT_FALSE(shower_->IsAppListVisible());
  EXPECT_FALSE(shower_->HasView());
  EXPECT_FALSE(HasKeepAlive());
}

TEST_F(AppListShowerUnitTest, ShowForProfilePutsViewOnScreen) {
  shower_->ShowForProfile(profile1_.get());
  EXPECT_TRUE(shower_->IsAppListVisible());
  EXPECT_TRUE(shower_->HasView());
  EXPECT_TRUE(HasKeepAlive());
}

TEST_F(AppListShowerUnitTest, HidingViewRemovesKeepalive) {
  shower_->ShowForProfile(profile1_.get());
  shower_->DismissAppList();
  EXPECT_FALSE(shower_->IsAppListVisible());
  EXPECT_TRUE(shower_->HasView());
  EXPECT_FALSE(HasKeepAlive());
}

TEST_F(AppListShowerUnitTest, HideAndShowReusesView) {
  EXPECT_EQ(0, views_created_);
  shower_->ShowForProfile(profile1_.get());
  EXPECT_EQ(1, views_created_);
  EXPECT_EQ(0, views_dismissed_);
  shower_->DismissAppList();
  EXPECT_EQ(1, views_dismissed_);
  shower_->ShowForProfile(profile1_.get());
  EXPECT_EQ(1, views_created_);
}

TEST_F(AppListShowerUnitTest, CloseAndShowRecreatesView) {
  shower_->ShowForProfile(profile1_.get());
  shower_->HandleViewBeingDestroyed();
  // Destroying implies hiding. A separate notification shouldn't go out.
  EXPECT_EQ(0, views_dismissed_);
  shower_->ShowForProfile(profile1_.get());
  EXPECT_EQ(2, views_created_);
}

TEST_F(AppListShowerUnitTest, CloseRemovesView) {
  shower_->ShowForProfile(profile1_.get());
  shower_->HandleViewBeingDestroyed();
  EXPECT_FALSE(shower_->IsAppListVisible());
  EXPECT_FALSE(shower_->HasView());
  EXPECT_FALSE(HasKeepAlive());
}

TEST_F(AppListShowerUnitTest, CloseAppListClearsProfile) {
  EXPECT_EQ(NULL, shower_->profile());
  shower_->ShowForProfile(profile1_.get());
  EXPECT_EQ(profile1_.get(), shower_->profile());
  shower_->HandleViewBeingDestroyed();
  EXPECT_EQ(NULL, shower_->profile());
}

TEST_F(AppListShowerUnitTest, SwitchingProfiles) {
  shower_->ShowForProfile(profile1_.get());
  EXPECT_EQ("p1", shower_->profile()->GetProfileUserName());
  shower_->ShowForProfile(profile2_.get());
  EXPECT_EQ("p2", shower_->profile()->GetProfileUserName());

  // Shouldn't create new view for second profile - it should switch in place.
  EXPECT_EQ(1, views_created_);
  EXPECT_EQ(0, views_dismissed_);
}
