// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceApi) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kAlternateErrorPagesEnabled, false);
  pref_service->SetBoolean(prefs::kAutofillEnabled, false);
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  pref_service->SetBoolean(prefs::kEnableHyperlinkAuditing, false);
  pref_service->SetBoolean(prefs::kEnableReferrers, false);
  pref_service->SetBoolean(prefs::kEnableTranslate, false);
  pref_service->SetBoolean(prefs::kInstantEnabled, false);
  pref_service->SetBoolean(prefs::kNetworkPredictionEnabled, false);
  pref_service->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  pref_service->SetBoolean(prefs::kSearchSuggestEnabled, false);

  EXPECT_TRUE(RunExtensionTest("preference/standard")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kAutofillEnabled));
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kEnableHyperlinkAuditing));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kEnableReferrers));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kEnableTranslate));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kInstantEnabled));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kNetworkPredictionEnabled));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  EXPECT_TRUE(pref_service->GetBoolean(prefs::kSearchSuggestEnabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferencePersistentIncognito) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/persistent_incognito")) <<
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

// Flakily times out: http://crbug.com/106144
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_PreferenceIncognitoDisabled) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  EXPECT_FALSE(RunExtensionTest("preference/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceSessionOnlyIncognito) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(
      RunExtensionTestIncognito("preference/session_only_incognito")) <<
      message_;

  EXPECT_TRUE(browser()->profile()->HasOffTheRecordProfile());

  PrefService* otr_prefs =
      browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  const PrefService::Preference* pref =
      otr_prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(otr_prefs->GetBoolean(prefs::kBlockThirdPartyCookies));

  pref = prefs->FindPreference(prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceClear) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kBlockThirdPartyCookies, true);

  EXPECT_TRUE(RunExtensionTest("preference/clear")) << message_;

  const PrefService::Preference* pref = pref_service->FindPreference(
      prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(true, pref_service->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceOnChange) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kBlockThirdPartyCookies, false);

  EXPECT_TRUE(RunExtensionTestIncognito("preference/onchange")) <<
      message_;
}
