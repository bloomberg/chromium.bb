// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_shower_views.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/scoped_keep_alive.h"
#include "chrome/browser/ui/app_list/test/fake_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAppListShower : public AppListShower {
 public:
  explicit FakeAppListShower(AppListShowerDelegate* delegate)
      : AppListShower(delegate), has_view_(false), visible_(false) {}

  // AppListShower:
  virtual void HandleViewBeingDestroyed() OVERRIDE {
    AppListShower::HandleViewBeingDestroyed();
    has_view_ = false;
    visible_ = false;
  }

  virtual bool IsAppListVisible() const OVERRIDE { return visible_; }

  virtual app_list::AppListView* MakeViewForCurrentProfile() OVERRIDE {
    has_view_ = true;
    return NULL;
  }

  virtual void UpdateViewForNewProfile() OVERRIDE {}

  virtual void Show() OVERRIDE {
    visible_ = true;
  }

  virtual void Hide() OVERRIDE {
    visible_ = false;
  }

  virtual bool HasView() const OVERRIDE {
    return has_view_;
  }

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

  virtual void SetUp() OVERRIDE {
    shower_.reset(new FakeAppListShower(this));
    profile1_ = CreateProfile("p1").Pass();
    profile2_ = CreateProfile("p2").Pass();
  }

  virtual void TearDown() OVERRIDE {
  }

  scoped_ptr<FakeProfile> CreateProfile(const std::string& name) {
    return make_scoped_ptr(new FakeProfile(name));
  }

  // AppListCreatorDelegate:
  virtual AppListControllerDelegate* GetControllerDelegateForCreate() OVERRIDE {
    return NULL;
  }

  bool HasKeepAlive() const {
    return shower_->keep_alive_.get() != NULL;
  }

  virtual void OnViewCreated() OVERRIDE { ++views_created_; }
  virtual void OnViewDismissed() OVERRIDE { ++views_dismissed_; }
  virtual void MoveNearCursor(app_list::AppListView* view) OVERRIDE {}

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
  EXPECT_EQ("p1", shower_->profile()->GetProfileName());
  shower_->ShowForProfile(profile2_.get());
  EXPECT_EQ("p2", shower_->profile()->GetProfileName());

  // Shouldn't create new view for second profile - it should switch in place.
  EXPECT_EQ(1, views_created_);
  EXPECT_EQ(0, views_dismissed_);
}
