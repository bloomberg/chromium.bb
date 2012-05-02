// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

namespace {

std::string kHomepage1 = "http://google.com/";
std::string kHomepage2 = "http://example.com/";

}

class HomepageChangeTest : public testing::Test {
  virtual void SetUp() OVERRIDE {
    prefs_ = profile_.GetPrefs();
  }

 protected:
  TestingProfile profile_;
  PrefService* prefs_;
};

TEST_F(HomepageChangeTest, InitAndApply) {
  prefs_->SetString(prefs::kHomePage, kHomepage1);

  // Create a change and apply it.
  scoped_ptr<BaseSettingChange> change(
      CreateHomepageChange(kHomepage1, false, true, kHomepage2, false, true));
  ASSERT_TRUE(change->Init(&profile_));
  // Setting is initially reverted to backup.
  EXPECT_EQ(kHomepage2, prefs_->GetString(prefs::kHomePage));

  change->Apply(NULL);  // |browser| is unused.
  // New setting active now.
  EXPECT_EQ(kHomepage1, prefs_->GetString(prefs::kHomePage));
}

TEST_F(HomepageChangeTest, InitAndDiscard) {
  prefs_->SetString(prefs::kHomePage, kHomepage1);

  // Create a change and apply it.
  scoped_ptr<BaseSettingChange> change(
      CreateHomepageChange(kHomepage1, false, true, kHomepage2, false, true));
  ASSERT_TRUE(change->Init(&profile_));
  // Setting is initially reverted to backup.
  EXPECT_EQ(kHomepage2, prefs_->GetString(prefs::kHomePage));

  change->Discard(NULL);  // |browser| is unused.
  // Nothing changed by Discard.
  EXPECT_EQ(kHomepage2, prefs_->GetString(prefs::kHomePage));
}

TEST_F(HomepageChangeTest, DiscardButtonCaptionsSameSetting) {
  // New and backup setting have the same URL (or both are NTP).

  // NTP can be represented both by an empty URL or a |homepage_is_ntp| flag.
  // NTP -> NTP.
  scoped_ptr<BaseSettingChange> change(
      CreateHomepageChange(kHomepage1, true, true, kHomepage2, true, true));
  ASSERT_TRUE(change->Init(&profile_));
  string16 generic_caption(l10n_util::GetStringUTF16(IDS_KEEP_SETTING));
  EXPECT_EQ(generic_caption, change->GetDiscardButtonText());

  // NTP -> NTP.
  change.reset(
      CreateHomepageChange("", false, true, kHomepage2, true, true));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(generic_caption, change->GetDiscardButtonText());

  // NTP -> NTP.
  change.reset(
      CreateHomepageChange(kHomepage1, true, true, "", false, true));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(generic_caption, change->GetDiscardButtonText());

  // kHomepage1 -> kHomepage1.
  change.reset(
      CreateHomepageChange(kHomepage1, false, true, kHomepage1, false, true));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(generic_caption, change->GetDiscardButtonText());
}

TEST_F(HomepageChangeTest, DiscardButtonCaptionsDifferentSettings) {
  // New and backup settings differ.

  // NTP -> kHomepage2.
  scoped_ptr<BaseSettingChange> change(
      CreateHomepageChange(kHomepage1, true, true, kHomepage2, false, true));
  ASSERT_TRUE(change->Init(&profile_));
  string16 generic_caption(l10n_util::GetStringUTF16(IDS_KEEP_SETTING));
  EXPECT_NE(generic_caption, change->GetDiscardButtonText());

  // kHomepage1 -> NTP.
  change.reset(
      CreateHomepageChange(kHomepage1, false, true, kHomepage2, true, true));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_NE(generic_caption, change->GetDiscardButtonText());

  // kHomepage1 -> kHomepage2.
  change.reset(
      CreateHomepageChange(kHomepage1, false, true, kHomepage2, false, true));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_NE(generic_caption, change->GetDiscardButtonText());
}

}  // namespace protector
