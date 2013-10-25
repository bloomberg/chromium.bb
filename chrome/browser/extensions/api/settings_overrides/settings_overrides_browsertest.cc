// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OverrideSettings) {
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetString(prefs::kHomePage, "http://google.com/");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("settings_override"));
  ASSERT_TRUE(extension);

  EXPECT_EQ("http://www.homepage.com/", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  UnloadExtension(extension->id());
  EXPECT_EQ("http://google.com/", prefs->GetString(prefs::kHomePage));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

}  // namespace
