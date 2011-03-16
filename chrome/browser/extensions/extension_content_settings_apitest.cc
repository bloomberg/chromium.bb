// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  EXPECT_TRUE(RunExtensionTest("content_settings/standard")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(RunExtensionTestIncognito("content_settings/incognito")) <<
      message_;

  // Setting an incognito preference should not create an incognito profile.
  EXPECT_FALSE(browser()->profile()->HasOffTheRecordProfile());

  PrefService* otr_prefs =
      browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_TRUE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabledContentSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  EXPECT_FALSE(RunExtensionTest("content_settings/incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettingsClear) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  EXPECT_TRUE(RunExtensionTest("content_settings/clear")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(true, pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContentSettingsOnChange) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(RunExtensionTestIncognito("content_settings/onchange")) <<
      message_;
}
