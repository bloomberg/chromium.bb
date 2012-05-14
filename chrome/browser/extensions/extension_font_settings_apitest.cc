// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FontSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyKorean, "Tahoma");
  prefs->SetString(prefs::kWebKitGlobalSansSerifFontFamily, "Arial");
  prefs->SetInteger(prefs::kWebKitGlobalDefaultFontSize, 16);
  prefs->SetInteger(prefs::kWebKitGlobalDefaultFixedFontSize, 14);
  prefs->SetInteger(prefs::kWebKitGlobalMinimumFontSize, 8);
  prefs->SetString(prefs::kGlobalDefaultCharset, "Shift_JIS");

  EXPECT_TRUE(RunExtensionTest("font_settings/standard")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FontSettingsIncognito) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kWebKitStandardFontFamilyKorean, "Tahoma");
  prefs->SetString(prefs::kWebKitGlobalSansSerifFontFamily, "Arial");
  prefs->SetInteger(prefs::kWebKitGlobalDefaultFontSize, 16);

  int flags = ExtensionApiTest::kFlagEnableIncognito |
      ExtensionApiTest::kFlagUseIncognito;
  EXPECT_TRUE(RunExtensionSubtest("font_settings/incognito",
                                  "launch.html",
                                  flags));
}
