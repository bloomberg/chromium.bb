// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dummy_pref_store.h"
#include "chrome/browser/managed_prefs_banner_base.h"
#include "chrome/browser/pref_service.h"
#include "chrome/common/pref_names.h"
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
    managed_prefs_ = new DummyPrefStore;
    extension_prefs_ = new DummyPrefStore;
    user_prefs_ = new DummyPrefStore;
    default_prefs_ = new DummyPrefStore;
    pref_service_.reset(new PrefService(new PrefValueStore(managed_prefs_,
                                                           extension_prefs_,
                                                           user_prefs_,
                                                           default_prefs_)));
    pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com");
    pref_service_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, false);
    pref_service_->RegisterBooleanPref(kDummyPref, false);
  }

  scoped_ptr<PrefService> pref_service_;
  DummyPrefStore* managed_prefs_;
  DummyPrefStore* extension_prefs_;
  DummyPrefStore* user_prefs_;
  DummyPrefStore* default_prefs_;
};

TEST_F(ManagedPrefsBannerBaseTest, VisibilityTest) {
  ManagedPrefsBannerBase banner(pref_service_.get(), OPTIONS_PAGE_GENERAL);
  EXPECT_FALSE(banner.DetermineVisibility());
  managed_prefs_->prefs()->SetBoolean(kDummyPref, true);
  EXPECT_FALSE(banner.DetermineVisibility());
  user_prefs_->prefs()->SetString(prefs::kHomePage, "http://foo.com");
  EXPECT_FALSE(banner.DetermineVisibility());
  managed_prefs_->prefs()->SetString(prefs::kHomePage, "http://bar.com");
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
