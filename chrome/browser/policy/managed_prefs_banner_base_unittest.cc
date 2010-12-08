// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_prefs_banner_base.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Tests whether managed preferences banner base functionality correctly
// determines banner visiblity.
class ManagedPrefsBannerBaseTest : public testing::Test {
 public:
  virtual void SetUp() {
    user_prefs_.reset(new TestingPrefService);
    user_prefs_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, false);
    user_prefs_->RegisterBooleanPref(prefs::kSearchSuggestEnabled, false);
    local_state_.reset(new TestingPrefService);
    local_state_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, false);
    local_state_->RegisterBooleanPref(prefs::kMetricsReportingEnabled, false);
  }

  scoped_ptr<TestingPrefService> local_state_;
  scoped_ptr<TestingPrefService> user_prefs_;
};

TEST_F(ManagedPrefsBannerBaseTest, VisibilityTest) {
  ManagedPrefsBannerBase banner(local_state_.get(),
                                user_prefs_.get(),
                                OPTIONS_PAGE_ADVANCED);
  banner.AddLocalStatePref(prefs::kMetricsReportingEnabled);
  EXPECT_FALSE(banner.DetermineVisibility());
  user_prefs_->SetManagedPref(prefs::kHomePageIsNewTabPage,
                              Value::CreateBooleanValue(true));
  EXPECT_FALSE(banner.DetermineVisibility());
  user_prefs_->SetUserPref(prefs::kSearchSuggestEnabled,
                           Value::CreateBooleanValue(true));
  EXPECT_FALSE(banner.DetermineVisibility());
  user_prefs_->SetManagedPref(prefs::kSearchSuggestEnabled,
                              Value::CreateBooleanValue(false));
  EXPECT_TRUE(banner.DetermineVisibility());
  local_state_->SetManagedPref(prefs::kMetricsReportingEnabled,
                               Value::CreateBooleanValue(true));
  EXPECT_TRUE(banner.DetermineVisibility());
  user_prefs_->RemoveManagedPref(prefs::kSearchSuggestEnabled);
  EXPECT_TRUE(banner.DetermineVisibility());
  local_state_->RemoveManagedPref(prefs::kMetricsReportingEnabled);
  EXPECT_FALSE(banner.DetermineVisibility());
  local_state_->SetManagedPref(prefs::kHomePageIsNewTabPage,
                               Value::CreateBooleanValue(true));
  EXPECT_FALSE(banner.DetermineVisibility());
}

// Mock class that allows to capture the notification callback.
class ManagedPrefsBannerBaseMock : public ManagedPrefsBannerBase {
 public:
  ManagedPrefsBannerBaseMock(PrefService* local_state,
                             PrefService* user_prefs,
                             OptionsPage page)
      : ManagedPrefsBannerBase(local_state, user_prefs, page) { }

  MOCK_METHOD0(OnUpdateVisibility, void());
};

TEST_F(ManagedPrefsBannerBaseTest, NotificationTest) {
  ManagedPrefsBannerBaseMock banner(local_state_.get(),
                                    user_prefs_.get(),
                                    OPTIONS_PAGE_ADVANCED);
  banner.AddLocalStatePref(prefs::kMetricsReportingEnabled);
  EXPECT_CALL(banner, OnUpdateVisibility()).Times(0);
  user_prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  EXPECT_CALL(banner, OnUpdateVisibility()).Times(1);
  user_prefs_->SetManagedPref(prefs::kSearchSuggestEnabled,
                              Value::CreateBooleanValue(false));
  EXPECT_CALL(banner, OnUpdateVisibility()).Times(1);
  local_state_->SetManagedPref(prefs::kMetricsReportingEnabled,
                               Value::CreateBooleanValue(true));
}

}  // namespace policy
