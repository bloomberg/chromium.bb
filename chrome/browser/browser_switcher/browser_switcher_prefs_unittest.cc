// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <algorithm>
#include <memory>

#include "base/values.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;

namespace browser_switcher {

namespace {

std::unique_ptr<base::Value> StringArrayToValue(
    const std::vector<const char*>& strings) {
  std::vector<base::Value> values(strings.size());
  std::transform(strings.begin(), strings.end(), values.begin(),
                 [](const char* s) { return base::Value(s); });
  return std::make_unique<base::Value>(values);
}

}  // namespace

class BrowserSwitcherPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    BrowserSwitcherPrefs::RegisterProfilePrefs(prefs_backend_.registry());
    prefs_ = std::make_unique<BrowserSwitcherPrefs>(&prefs_backend_);
  }

  sync_preferences::TestingPrefServiceSyncable* prefs_backend() {
    return &prefs_backend_;
  }
  BrowserSwitcherPrefs* prefs() { return prefs_.get(); }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_backend_;
  std::unique_ptr<BrowserSwitcherPrefs> prefs_;
};

TEST_F(BrowserSwitcherPrefsTest, ListensForPrefChanges) {
  prefs_backend()->SetManagedPref(prefs::kEnabled,
                                  std::make_unique<base::Value>(true));
  prefs_backend()->SetManagedPref(prefs::kAlternativeBrowserPath,
                                  std::make_unique<base::Value>("notepad.exe"));
  prefs_backend()->SetManagedPref(prefs::kAlternativeBrowserParameters,
                                  StringArrayToValue({"a", "b", "c"}));
  prefs_backend()->SetManagedPref(prefs::kUrlList,
                                  StringArrayToValue({"example.com"}));
  prefs_backend()->SetManagedPref(prefs::kUrlGreylist,
                                  StringArrayToValue({"foo.example.com"}));

  EXPECT_EQ(true, prefs()->IsEnabled());

  EXPECT_EQ("notepad.exe", prefs()->GetAlternativeBrowserPath());

  EXPECT_EQ(3u, prefs()->GetAlternativeBrowserParameters().size());
  EXPECT_EQ("a", prefs()->GetAlternativeBrowserParameters()[0]);
  EXPECT_EQ("b", prefs()->GetAlternativeBrowserParameters()[1]);
  EXPECT_EQ("c", prefs()->GetAlternativeBrowserParameters()[2]);

  EXPECT_EQ(1u, prefs()->GetRules().sitelist.size());
  EXPECT_EQ("example.com", prefs()->GetRules().sitelist[0]);

  EXPECT_EQ(1u, prefs()->GetRules().greylist.size());
  EXPECT_EQ("foo.example.com", prefs()->GetRules().greylist[0]);
}

}  // namespace browser_switcher
