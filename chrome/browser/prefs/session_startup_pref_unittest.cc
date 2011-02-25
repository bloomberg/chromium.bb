// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for SessionStartupPref.
class SessionStartupPrefTest : public testing::Test {
 public:
  virtual void SetUp() {
    pref_service_.reset(new TestingPrefService);
    SessionStartupPref::RegisterUserPrefs(pref_service_.get());
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
