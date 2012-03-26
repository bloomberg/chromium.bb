// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

namespace {

const char kStartupUrl1[] = "http://google.com";
const char kStartupUrl2[] = "http://example.com";

}  // namespace

class SessionStartupChangeTest : public testing::Test {
 public:
  SessionStartupChangeTest()
      : initial_startup_pref_(SessionStartupPref::DEFAULT) {
  }

  virtual void SetUp() OVERRIDE {
    // Ensure initial session startup pref.
    SessionStartupPref::SetStartupPref(&profile_, initial_startup_pref_);
  }

 protected:
  TestingProfile profile_;
  SessionStartupPref initial_startup_pref_;
  PinnedTabCodec::Tabs empty_pinned_tabs_;
};

TEST_F(SessionStartupChangeTest, InitAndApply) {
  // Create a change and apply it.
  SessionStartupPref backup_startup_pref(SessionStartupPref::LAST);
  scoped_ptr<BaseSettingChange> change(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  // Setting is initially reverted to backup.
  EXPECT_EQ(SessionStartupPref::LAST,
            SessionStartupPref::GetStartupPref(&profile_).type);
  change->Apply(NULL);  // |browser| is unused.
  // New setting active now.
  EXPECT_EQ(SessionStartupPref::DEFAULT,
            SessionStartupPref::GetStartupPref(&profile_).type);
}

TEST_F(SessionStartupChangeTest, InitAndDiscard) {
  // Create a change and discard it.
  SessionStartupPref backup_startup_pref(SessionStartupPref::LAST);
  scoped_ptr<BaseSettingChange> change(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  // Setting is initially reverted to backup.
  EXPECT_EQ(SessionStartupPref::LAST,
            SessionStartupPref::GetStartupPref(&profile_).type);
  change->Discard(NULL);  // |browser| is unused.
  // Nothing changed by Discard.
  EXPECT_EQ(SessionStartupPref::LAST,
            SessionStartupPref::GetStartupPref(&profile_).type);
}

TEST_F(SessionStartupChangeTest, ApplyButtonCaptions) {
  // Apply button captions for "Open NTP" and "Open specific URLs" cases.
  string16 open_ntp_caption =
      l10n_util::GetStringUTF16(IDS_CHANGE_STARTUP_SETTINGS_NTP);
  string16 open_url1_etc_caption =
      l10n_util::GetStringFUTF16(IDS_CHANGE_STARTUP_SETTINGS_URLS,
                                 UTF8ToUTF16(GURL(kStartupUrl1).host()));
  string16 open_url2_etc_caption =
      l10n_util::GetStringFUTF16(IDS_CHANGE_STARTUP_SETTINGS_URLS,
                                 UTF8ToUTF16(GURL(kStartupUrl2).host()));

  // Open NTP.
  initial_startup_pref_.type = SessionStartupPref::DEFAULT;
  SessionStartupPref backup_startup_pref(SessionStartupPref::DEFAULT);
  scoped_ptr<BaseSettingChange> change(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_ntp_caption, change->GetApplyButtonText());

  // Pinned tabs count as startup URLs as well.
  PinnedTabCodec::Tabs new_pinned_tabs;
  BrowserInit::LaunchWithProfile::Tab pinned_tab;
  pinned_tab.url = GURL(kStartupUrl2);
  new_pinned_tabs.push_back(pinned_tab);
  change.reset(
      CreateSessionStartupChange(initial_startup_pref_, new_pinned_tabs,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_url2_etc_caption, change->GetApplyButtonText());

  // "Open URLs" with no URLs is the same as "Open NTP".
  initial_startup_pref_.type = SessionStartupPref::URLS;
  change.reset(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_ntp_caption, change->GetApplyButtonText());

  // Single URL.
  initial_startup_pref_.urls.push_back(GURL(kStartupUrl1));
  change.reset(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_url1_etc_caption, change->GetApplyButtonText());

  // Multiple URLs: name of the first one used.
  initial_startup_pref_.urls.push_back(GURL(kStartupUrl2));
  change.reset(
      CreateSessionStartupChange(initial_startup_pref_, empty_pinned_tabs_,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_url1_etc_caption, change->GetApplyButtonText());

  // Pinned tabs go after the startup URLs.
  change.reset(
      CreateSessionStartupChange(initial_startup_pref_, new_pinned_tabs,
                                 backup_startup_pref, empty_pinned_tabs_));
  ASSERT_TRUE(change->Init(&profile_));
  EXPECT_EQ(open_url1_etc_caption, change->GetApplyButtonText());
}

}  // namespace protector
