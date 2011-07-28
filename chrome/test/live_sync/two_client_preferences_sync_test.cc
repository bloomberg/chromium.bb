// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/preferences_helper.h"

class TwoClientPreferencesSyncTest : public LiveSyncTest {
 public:
  TwoClientPreferencesSyncTest() : LiveSyncTest(TWO_CLIENT) {}
  virtual ~TwoClientPreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientPreferencesSyncTest);
};

// TCM ID - 7306186.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kHomePageIsNewTabPage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
}

// TCM ID - 7260488.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));

  PreferencesHelper::ChangeStringPref(0, prefs::kHomePage,
                                      "http://www.google.com/0");
  PreferencesHelper::ChangeStringPref(1, prefs::kHomePage,
                                      "http://www.google.com/1");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));
}

// TCM ID - 3649278.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kPasswordManagerEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kPasswordManagerEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));
}

// TCM ID - 3699293.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kKeepEverythingSynced) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kKeepEverythingSynced));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncThemes));

  GetClient(0)->DisableSyncForDatatype(syncable::THEMES);
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kKeepEverythingSynced));
}

// TCM ID - 3661290.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, DisablePreferences) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncPreferences));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));

  GetClient(1)->DisableSyncForDatatype(syncable::PREFERENCES);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kPasswordManagerEnabled);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));

  GetClient(1)->EnableSyncForDatatype(syncable::PREFERENCES);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));
}

// TCM ID - 3664292.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncPreferences));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

  GetClient(1)->DisableSyncForAllDatatypes();
  PreferencesHelper::ChangeBooleanPref(0, prefs::kPasswordManagerEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitSyncCycleCompletion("Changed a preference."));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));

  PreferencesHelper::ChangeBooleanPref(1, prefs::kShowHomeButton);
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

  GetClient(1)->EnableSyncForAllDatatypes();
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kPasswordManagerEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}

// TCM ID - 3604297.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, SignInDialog) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncPreferences));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncBookmarks));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncThemes));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncExtensions));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncAutofill));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kKeepEverythingSynced));

  GetClient(0)->DisableSyncForDatatype(syncable::PREFERENCES);
  GetClient(1)->EnableSyncForDatatype(syncable::PREFERENCES);
  GetClient(0)->DisableSyncForDatatype(syncable::AUTOFILL);
  GetClient(1)->EnableSyncForDatatype(syncable::AUTOFILL);
  GetClient(0)->DisableSyncForDatatype(syncable::BOOKMARKS);
  GetClient(1)->EnableSyncForDatatype(syncable::BOOKMARKS);
  GetClient(0)->DisableSyncForDatatype(syncable::EXTENSIONS);
  GetClient(1)->EnableSyncForDatatype(syncable::EXTENSIONS);
  GetClient(0)->DisableSyncForDatatype(syncable::THEMES);
  GetClient(1)->EnableSyncForDatatype(syncable::THEMES);

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncPreferences));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncBookmarks));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncThemes));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncExtensions));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kSyncAutofill));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kKeepEverythingSynced));
}

// TCM ID - 3666296.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kShowBookmarkBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowBookmarkBar));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowBookmarkBar);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowBookmarkBar));
}

// TCM ID - 3611311.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kCheckDefaultBrowser) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kCheckDefaultBrowser));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kCheckDefaultBrowser);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kCheckDefaultBrowser));
}

// TCM ID - 3628298.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kHomePage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));

  PreferencesHelper::ChangeStringPref(0, prefs::kHomePage,
                                      "http://news.google.com");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));
}

// TCM ID - 7297269.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kShowHomeButton) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}

// TCM ID - 3710285.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kEnableTranslate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableTranslate));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kEnableTranslate);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableTranslate));
}

// TCM ID - 3664293.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kAutofillEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kAutofillEnabled));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kAutofillEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kAutofillEnabled));
}

// TCM ID - 3632259.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kURLsToRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::IntegerPrefMatches(prefs::kRestoreOnStartup));
  ASSERT_TRUE(PreferencesHelper::ListPrefMatches(
      prefs::kURLsToRestoreOnStartup));

  PreferencesHelper::ChangeIntegerPref(0, prefs::kRestoreOnStartup, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IntegerPrefMatches(prefs::kRestoreOnStartup));

  ListValue urls;
  urls.Append(Value::CreateStringValue("http://www.google.com/"));
  urls.Append(Value::CreateStringValue("http://www.flickr.com/"));
  PreferencesHelper::ChangeIntegerPref(0, prefs::kRestoreOnStartup, 4);
  PreferencesHelper::ChangeListPref(0, prefs::kURLsToRestoreOnStartup, urls);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IntegerPrefMatches(prefs::kRestoreOnStartup));
  ASSERT_TRUE(PreferencesHelper::ListPrefMatches(
      prefs::kURLsToRestoreOnStartup));
}

// TCM ID - 3684287.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::IntegerPrefMatches(prefs::kRestoreOnStartup));

  PreferencesHelper::ChangeIntegerPref(0, prefs::kRestoreOnStartup, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IntegerPrefMatches(prefs::kRestoreOnStartup));
}

// TCM ID - 3703314.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, Privacy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kAlternateErrorPagesEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSearchSuggestEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kNetworkPredictionEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSafeBrowsingEnabled));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kAlternateErrorPagesEnabled);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kSearchSuggestEnabled);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kNetworkPredictionEnabled);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kSafeBrowsingEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kAlternateErrorPagesEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSearchSuggestEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kNetworkPredictionEnabled));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSafeBrowsingEnabled));
}

// TCM ID - 3649279.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, ClearData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kDeleteBrowsingHistory));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kDeleteDownloadHistory));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteCache));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteCookies));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeletePasswords));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteFormData));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeleteBrowsingHistory);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeleteDownloadHistory);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeleteCache);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeleteCookies);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeletePasswords);
  PreferencesHelper::ChangeBooleanPref(0, prefs::kDeleteFormData);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kDeleteBrowsingHistory));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kDeleteDownloadHistory));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteCache));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteCookies));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeletePasswords));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kDeleteFormData));
}

// TCM ID - 3686300.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kWebKitUsesUniversalDetector) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kWebKitUsesUniversalDetector));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kWebKitUsesUniversalDetector);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kWebKitUsesUniversalDetector));
}

// TCM ID - 3673298.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kDefaultCharset) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kDefaultCharset));

  PreferencesHelper::ChangeStringPref(0, prefs::kDefaultCharset, "Thai");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kDefaultCharset));
}

// TCM ID - 3653296.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kBlockThirdPartyCookies) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kBlockThirdPartyCookies));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kBlockThirdPartyCookies);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kBlockThirdPartyCookies));
}

// TCM ID - 7297279.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kClearSiteDataOnExit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kClearSiteDataOnExit));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kClearSiteDataOnExit);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kClearSiteDataOnExit));
}

// TCM ID - 7306184.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kSafeBrowsingEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSafeBrowsingEnabled));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kSafeBrowsingEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kSafeBrowsingEnabled));
}

// TCM ID - 3624302.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kAutofillAuxiliaryProfilesEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kAutofillAuxiliaryProfilesEnabled));

  PreferencesHelper::ChangeBooleanPref(0,
      prefs::kAutofillAuxiliaryProfilesEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  // kAutofillAuxiliaryProfilesEnabled is only synced on Mac.
#if defined(OS_MACOSX)
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kAutofillAuxiliaryProfilesEnabled));
#else
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(
      prefs::kAutofillAuxiliaryProfilesEnabled));
#endif  // OS_MACOSX
}

// TCM ID - 3717298.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kPromptForDownload) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kPromptForDownload));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kPromptForDownload);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kPromptForDownload));
}

// TCM ID - 3729263.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kPrefTranslateLanguageBlacklist) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableTranslate));

  TranslatePrefs translate_client0_prefs(PreferencesHelper::GetPrefs(0));
  TranslatePrefs translate_client1_prefs(PreferencesHelper::GetPrefs(1));
  ASSERT_FALSE(translate_client0_prefs.IsLanguageBlacklisted("fr"));
  translate_client0_prefs.BlacklistLanguage("fr");
  ASSERT_TRUE(translate_client0_prefs.IsLanguageBlacklisted("fr"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(translate_client1_prefs.IsLanguageBlacklisted("fr"));

  translate_client0_prefs.RemoveLanguageFromBlacklist("fr");
  ASSERT_FALSE(translate_client0_prefs.IsLanguageBlacklisted("fr"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_FALSE(translate_client1_prefs.IsLanguageBlacklisted("fr"));
}

// TCM ID - 7307195.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kPrefTranslateWhitelists) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableTranslate));

  TranslatePrefs translate_client0_prefs(PreferencesHelper::GetPrefs(0));
  TranslatePrefs translate_client1_prefs(PreferencesHelper::GetPrefs(1));
  ASSERT_FALSE(translate_client0_prefs.IsLanguagePairWhitelisted("en", "bg"));
  translate_client0_prefs.WhitelistLanguagePair("en", "bg");
  ASSERT_TRUE(translate_client0_prefs.IsLanguagePairWhitelisted("en", "bg"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(translate_client1_prefs.IsLanguagePairWhitelisted("en", "bg"));

  translate_client0_prefs.RemoveLanguagePairFromWhitelist("en", "bg");
  ASSERT_FALSE(translate_client0_prefs.IsLanguagePairWhitelisted("en", "bg"));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_FALSE(translate_client1_prefs.IsLanguagePairWhitelisted("en", "bg"));
}

// TCM ID - 3625298.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kPrefTranslateSiteBlacklist) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableTranslate));

  GURL url("http://www.google.com");
  std::string host(url.host());
  TranslatePrefs translate_client0_prefs(PreferencesHelper::GetPrefs(0));
  TranslatePrefs translate_client1_prefs(PreferencesHelper::GetPrefs(1));
  ASSERT_FALSE(translate_client0_prefs.IsSiteBlacklisted(host));
  translate_client0_prefs.BlacklistSite(host);
  ASSERT_TRUE(translate_client0_prefs.IsSiteBlacklisted(host));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(translate_client1_prefs.IsSiteBlacklisted(host));

  translate_client0_prefs.RemoveSiteFromBlacklist(host);
  ASSERT_FALSE(translate_client0_prefs.IsSiteBlacklisted(host));

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_FALSE(translate_client1_prefs.IsSiteBlacklisted(host));
}

// TCM ID - 6515252.
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kExtensionsUIDeveloperMode) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kExtensionsUIDeveloperMode));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kExtensionsUIDeveloperMode);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kExtensionsUIDeveloperMode));
}

// TCM ID - 7583816
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kAcceptLanguages) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  DisableVerifier();
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));

  PreferencesHelper::AppendStringPref(0, prefs::kAcceptLanguages, ",ar");
  PreferencesHelper::AppendStringPref(1, prefs::kAcceptLanguages, ",fr");
  ASSERT_TRUE(AwaitQuiescence());
  // kAcceptLanguages is not synced on Mac.
#if !defined(OS_MACOSX)
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#else
  ASSERT_FALSE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#endif  // OS_MACOSX

  PreferencesHelper::ChangeStringPref(0, prefs::kAcceptLanguages, "en-US");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
#if !defined(OS_MACOSX)
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#else
  ASSERT_FALSE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#endif  // OS_MACOSX

  PreferencesHelper::ChangeStringPref(0, prefs::kAcceptLanguages, "ar,en-US");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
#if !defined(OS_MACOSX)
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#else
  ASSERT_FALSE(PreferencesHelper::StringPrefMatches(prefs::kAcceptLanguages));
#endif  // OS_MACOSX
}

// TCM ID - 7590682
#if defined(TOOLKIT_USES_GTK)
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kUsesSystemTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kUsesSystemTheme));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kUsesSystemTheme);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_FALSE(PreferencesHelper::BooleanPrefMatches(prefs::kUsesSystemTheme));
}
#endif  // TOOLKIT_USES_GTK

// TCM ID - 3636292
#if defined(TOOLKIT_USES_GTK)
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       kUseCustomChromeFrame) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kUseCustomChromeFrame));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kUseCustomChromeFrame);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kUseCustomChromeFrame));
}
#endif  // TOOLKIT_USES_GTK

// TCM ID - 6473347.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kTapToClickEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kTapToClickEnabled));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kTapToClickEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kTapToClickEnabled));

  PreferencesHelper::ChangeBooleanPref(1, prefs::kTapToClickEnabled);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kTapToClickEnabled));
}
#endif  // OS_CHROMEOS

// TCM ID - 6458824.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest, kEnableScreenLock) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableScreenLock));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kEnableScreenLock);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableScreenLock));

  PreferencesHelper::ChangeBooleanPref(1, prefs::kEnableScreenLock);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kEnableScreenLock));
}
#endif  // OS_CHROMEOS

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(PreferencesHelper::EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(0));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(PreferencesHelper::EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(0));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(1));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_TRUE(PreferencesHelper::EnableEncryption(0));
  ASSERT_TRUE(PreferencesHelper::EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(0));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));

  ASSERT_TRUE(PreferencesHelper::EnableEncryption(0));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  PreferencesHelper::ChangeStringPref(1, prefs::kHomePage,
                                      "http://www.google.com/1");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(0));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(1));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
  ASSERT_TRUE(PreferencesHelper::StringPrefMatches(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_F(TwoClientPreferencesSyncTest,
                       SingleClientEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));

  PreferencesHelper::ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(PreferencesHelper::EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(0));
  ASSERT_TRUE(PreferencesHelper::IsEncrypted(1));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));

  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(prefs::kShowHomeButton));
}
