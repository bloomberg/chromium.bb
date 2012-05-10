// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/window_restore_utils.h"
#endif

// Unit tests for SessionStartupPref.
class SessionStartupPrefTest : public testing::Test {
 public:
  virtual void SetUp() {
    pref_service_.reset(new TestingPrefService);
    SessionStartupPref::RegisterUserPrefs(pref_service_.get());
    pref_service_->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true);
  }

  bool IsUseLastOpenDefault() {
    // On ChromeOS and OS X 10.7+, the default SessionStartupPref is LAST.
#if defined(OS_CHROMEOS)
    return true;
#elif defined(OS_MACOSX)
    return restore_utils::IsWindowRestoreEnabled();
#else
    return false;
#endif
  }

  scoped_ptr<TestingPrefService> pref_service_;
};

TEST_F(SessionStartupPrefTest, URLListIsFixedUp) {
  ListValue* url_pref_list = new ListValue;
  url_pref_list->Set(0, Value::CreateStringValue("google.com"));
  url_pref_list->Set(1, Value::CreateStringValue("chromium.org"));
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup, url_pref_list);

  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(2u, result.urls.size());
  EXPECT_EQ("http://google.com/", result.urls[0].spec());
  EXPECT_EQ("http://chromium.org/", result.urls[1].spec());
}

TEST_F(SessionStartupPrefTest, URLListManagedOverridesUser) {
  ListValue* url_pref_list1 = new ListValue;
  url_pref_list1->Set(0, Value::CreateStringValue("chromium.org"));
  pref_service_->SetUserPref(prefs::kURLsToRestoreOnStartup, url_pref_list1);

  ListValue* url_pref_list2 = new ListValue;
  url_pref_list2->Set(0, Value::CreateStringValue("chromium.org"));
  url_pref_list2->Set(1, Value::CreateStringValue("chromium.org"));
  url_pref_list2->Set(2, Value::CreateStringValue("chromium.org"));
  pref_service_->SetManagedPref(prefs::kURLsToRestoreOnStartup,
                                url_pref_list2);

  SessionStartupPref result =
      SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(3u, result.urls.size());

  SessionStartupPref override_test =
      SessionStartupPref(SessionStartupPref::URLS);
  override_test.urls.push_back(GURL("dev.chromium.org"));
  SessionStartupPref::SetStartupPref(pref_service_.get(), override_test);

  result = SessionStartupPref::GetStartupPref(pref_service_.get());
  EXPECT_EQ(3u, result.urls.size());
}

// Checks to make sure that if the user had previously not selected anything
// (so that, in effect, the default value "Open the homepage" was selected),
// their preferences are migrated on upgrade to m19.
TEST_F(SessionStartupPrefTest, DefaultMigration) {
  pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com/");
  pref_service_->SetString(prefs::kHomePage, "http://chromium.org/");
  pref_service_->SetBoolean(prefs::kHomePageIsNewTabPage, false);

  EXPECT_FALSE(pref_service_->HasPrefPath(prefs::kRestoreOnStartup));

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      pref_service_.get());

  if (IsUseLastOpenDefault()) {
    EXPECT_EQ(SessionStartupPref::LAST, pref.type);
    EXPECT_EQ(0U, pref.urls.size());
  } else {
    EXPECT_EQ(SessionStartupPref::URLS, pref.type);
    EXPECT_EQ(1U, pref.urls.size());
    EXPECT_EQ(GURL("http://chromium.org/"), pref.urls[0]);
  }
}

// Checks to make sure that if the user had previously not selected anything
// (so that, in effect, the default value "Open the homepage" was selected),
// and the NTP is being used for the homepage, their preferences are migrated
// to "Open the New Tab Page" on upgrade to M19.
TEST_F(SessionStartupPrefTest, DefaultMigrationHomepageIsNTP) {
  pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com/");
  pref_service_->SetString(prefs::kHomePage, "http://chromium.org/");
  pref_service_->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  EXPECT_FALSE(pref_service_->HasPrefPath(prefs::kRestoreOnStartup));

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      pref_service_.get());

  if (IsUseLastOpenDefault())
    EXPECT_EQ(SessionStartupPref::LAST, pref.type);
  else
    EXPECT_EQ(SessionStartupPref::DEFAULT, pref.type);

  // The "URLs to restore on startup" shouldn't get migrated.
  EXPECT_EQ(0U, pref.urls.size());
}

// Checks to make sure that if the user had previously selected "Open the
// "homepage", their preferences are migrated on upgrade to M19.
TEST_F(SessionStartupPrefTest, HomePageMigration) {
  pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com/");

  // By design, it's impossible to set the 'restore on startup' pref to 0
  // ("open the homepage") using SessionStartupPref::SetStartupPref(), so set it
  // using the pref service directly.
  pref_service_->SetInteger(prefs::kRestoreOnStartup, /*kPrefValueHomePage*/ 0);
  pref_service_->SetString(prefs::kHomePage, "http://chromium.org/");
  pref_service_->SetBoolean(prefs::kHomePageIsNewTabPage, false);

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      pref_service_.get());

  EXPECT_EQ(SessionStartupPref::URLS, pref.type);
  EXPECT_EQ(1U, pref.urls.size());
  EXPECT_EQ(GURL("http://chromium.org/"), pref.urls[0]);
}

// Checks to make sure that if the user had previously selected "Open the
// "homepage", and the NTP is being used for the homepage, their preferences
// are migrated on upgrade to M19.
TEST_F(SessionStartupPrefTest, HomePageMigrationHomepageIsNTP) {
  pref_service_->RegisterStringPref(prefs::kHomePage, "http://google.com/");

  // By design, it's impossible to set the 'restore on startup' pref to 0
  // ("open the homepage") using SessionStartupPref::SetStartupPref(), so set it
  // using the pref service directly.
  pref_service_->SetInteger(prefs::kRestoreOnStartup, /*kPrefValueHomePage*/ 0);
  pref_service_->SetString(prefs::kHomePage, "http://chromium.org/");
  pref_service_->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  SessionStartupPref pref = SessionStartupPref::GetStartupPref(
      pref_service_.get());

  EXPECT_EQ(SessionStartupPref::DEFAULT, pref.type);
}
