// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "webkit/plugins/npapi/mock_plugin_list.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceApi) {
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
  EXPECT_FALSE(RunExtensionTest("preference/persistent_incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceSessionOnlyIncognito) {
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
  EXPECT_TRUE(RunExtensionTestIncognito("preference/onchange")) <<
      message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PreferenceOnChangeSplit) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToProfile(
      browser()->profile()->GetOffTheRecordProfile());

  // Open an incognito window.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(),
                                     GURL("chrome://newtab/"));

  // changeDefault listeners.
  ExtensionTestMessageListener listener1("changeDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito1(
      "changeDefault incognito ready", true);

  // changeIncognitoOnly listeners.
  ExtensionTestMessageListener listener2(
      "changeIncognitoOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito2(
      "changeIncognitoOnly incognito ready", true);
  ExtensionTestMessageListener listener3(
      "changeIncognitoOnly regular listening", true);
  ExtensionTestMessageListener listener_incognito3(
      "changeIncognitoOnly incognito pref set", false);

  // changeDefaultOnly listeners.
  ExtensionTestMessageListener listener4(
      "changeDefaultOnly regular ready", true);
  ExtensionTestMessageListener listener_incognito4(
      "changeDefaultOnly incognito ready", true);
  ExtensionTestMessageListener listener5(
      "changeDefaultOnly regular pref set", false);
  ExtensionTestMessageListener listener_incognito5(
      "changeDefaultOnly incognito listening", true);

  // changeIncognitoOnlyBack listeners.
  ExtensionTestMessageListener listener6(
      "changeIncognitoOnlyBack regular ready", true);
  ExtensionTestMessageListener listener_incognito6(
      "changeIncognitoOnlyBack incognito ready", true);
  ExtensionTestMessageListener listener7(
      "changeIncognitoOnlyBack regular listening", true);
  ExtensionTestMessageListener listener_incognito7(
      "changeIncognitoOnlyBack incognito pref set", false);

  // clearIncognito listeners.
  ExtensionTestMessageListener listener8(
      "clearIncognito regular ready", true);
  ExtensionTestMessageListener listener_incognito8(
      "clearIncognito incognito ready", true);
  ExtensionTestMessageListener listener9(
      "clearIncognito regular listening", true);
  ExtensionTestMessageListener listener_incognito9(
      "clearIncognito incognito pref cleared", false);

  // clearDefault listeners.
  ExtensionTestMessageListener listener10(
      "clearDefault regular ready", true);
  ExtensionTestMessageListener listener_incognito10(
      "clearDefault incognito ready", true);

  base::FilePath extension_data_dir =
      test_data_dir_.AppendASCII("preference").AppendASCII("onchange_split");
  ASSERT_TRUE(LoadExtensionIncognito(extension_data_dir));

  // Test 1 - changeDefault
  EXPECT_TRUE(listener1.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito1.WaitUntilSatisfied()); // Incognito ready
  listener1.Reply("ok");
  listener_incognito1.Reply("ok");

  // Test 2 - changeIncognitoOnly
  EXPECT_TRUE(listener2.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito2.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener3.WaitUntilSatisfied()); // Regular listening
  listener2.Reply("ok");
  listener_incognito2.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito3.WaitUntilSatisfied());
  listener3.Reply("ok");

  // Test 3 - changeDefaultOnly
  EXPECT_TRUE(listener4.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito4.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener_incognito5.WaitUntilSatisfied()); // Incognito listening
  listener4.Reply("ok");
  listener_incognito4.Reply("ok");
  // Regular preference set - notify the incognito listener
  EXPECT_TRUE(listener5.WaitUntilSatisfied());
  listener_incognito5.Reply("ok");

  // Test 4 - changeIncognitoOnlyBack
  EXPECT_TRUE(listener6.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito6.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener7.WaitUntilSatisfied()); // Regular listening
  listener6.Reply("ok");
  listener_incognito6.Reply("ok");
  // Incognito preference set -- notify the regular listener
  EXPECT_TRUE(listener_incognito7.WaitUntilSatisfied());
  listener7.Reply("ok");

  // Test 5 - clearIncognito
  EXPECT_TRUE(listener8.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito8.WaitUntilSatisfied()); // Incognito ready
  EXPECT_TRUE(listener9.WaitUntilSatisfied()); // Regular listening
  listener8.Reply("ok");
  listener_incognito8.Reply("ok");
  // Incognito preference cleared -- notify the regular listener
  EXPECT_TRUE(listener_incognito9.WaitUntilSatisfied());
  listener9.Reply("ok");

  // Test 6 - clearDefault
  EXPECT_TRUE(listener10.WaitUntilSatisfied()); // Regular ready
  EXPECT_TRUE(listener_incognito10.WaitUntilSatisfied()); // Incognito ready
  listener10.Reply("ok");
  listener_incognito10.Reply("ok");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}
