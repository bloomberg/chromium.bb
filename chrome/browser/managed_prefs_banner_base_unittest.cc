// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dummy_pref_store.h"
#include "chrome/browser/managed_prefs_banner_base.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t* kDummyPref = L"dummy";

}  // namespace

// Tests whether managed preferences banner base functionality correctly
// determines banner visiblity.
class ManagedPrefsBannerBaseTest : public testing::Test {
 public:
  virtual void SetUp() {
    pref_service_.reset(new TestingPrefService);
    pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com");
    pref_service_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, false);
    pref_service_->RegisterBooleanPref(kDummyPref, false);
  }

  scoped_ptr<TestingPrefService> pref_service_;
};

TEST_F(ManagedPrefsBannerBaseTest, VisibilityTest) {
  ManagedPrefsBannerBase banner(pref_service_.get(), OPTIONS_PAGE_GENERAL);
  EXPECT_FALSE(banner.DetermineVisibility());
  pref_service_->SetManagedPref(kDummyPref, Value::CreateBooleanValue(true));
  EXPECT_FALSE(banner.DetermineVisibility());
  pref_service_->SetUserPref(prefs::kHomePage,
                             Value::CreateStringValue("http://foo.com"));
  EXPECT_FALSE(banner.DetermineVisibility());
  pref_service_->SetManagedPref(prefs::kHomePage,
                                Value::CreateStringValue("http://bar.com"));
  EXPECT_TRUE(banner.DetermineVisibility());
}

// Mock class that allows to capture the notification callback.
class ManagedPrefsBannerBaseMock : public ManagedPrefsBannerBase {
 public:
  ManagedPrefsBannerBaseMock(PrefService* pref_service, OptionsPage page)
      : ManagedPrefsBannerBase(pref_service, page) { }

  MOCK_METHOD0(OnUpdateVisibility, void());
};

TEST_F(ManagedPrefsBannerBaseTest, NotificationTest) {
  ManagedPrefsBannerBaseMock banner(pref_service_.get(), OPTIONS_PAGE_GENERAL);
  EXPECT_CALL(banner, OnUpdateVisibility()).Times(0);
  pref_service_->SetBoolean(kDummyPref, true);
  EXPECT_CALL(banner, OnUpdateVisibility()).Times(1);
  pref_service_->SetString(prefs::kHomePage, "http://foo.com");
}
