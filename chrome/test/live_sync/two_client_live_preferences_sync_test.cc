// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

// TCM ID - 7306186.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kHomePageIsNewTabPage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));

  bool new_kHomePageIsNewTabPage = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
      new_kHomePageIsNewTabPage);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage,
      new_kHomePageIsNewTabPage);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));
}

// TCM ID - 7260488.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetPrefs(0)->SetString(prefs::kHomePage, "http://www.google.com/1");
  GetPrefs(1)->SetString(prefs::kHomePage, "http://www.google.com/2");
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}

// TCM ID - 3649278.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kPasswordManagerEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  bool new_kPasswordManagerEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kPasswordManagerEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  GetPrefs(0)->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));
}

// TCM ID - 3699293.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kKeepEverythingSynced) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
            GetPrefs(1)->GetBoolean(prefs::kKeepEverythingSynced));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));

  GetClient(0)->DisableSyncForDatatype(syncable::THEMES);

  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
            GetPrefs(1)->GetBoolean(prefs::kKeepEverythingSynced));
}

// TCM ID - 3661290.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, DisablePreferences) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  GetClient(1)->DisableSyncForDatatype(syncable::PREFERENCES);
  bool new_kPasswordManagerEnabled = !GetPrefs(0)->GetBoolean(
      prefs::kPasswordManagerEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  GetPrefs(0)->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  GetClient(1)->EnableSyncForDatatype(syncable::PREFERENCES);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetVerifierPrefs()->GetBoolean(prefs::kPasswordManagerEnabled));
}

// TCM ID - 3604297.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, SignInDialog) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncBookmarks),
            GetPrefs(1)->GetBoolean(prefs::kSyncBookmarks));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncExtensions),
            GetPrefs(1)->GetBoolean(prefs::kSyncExtensions));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncAutofill),
            GetPrefs(1)->GetBoolean(prefs::kSyncAutofill));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
            GetPrefs(1)->GetBoolean(prefs::kKeepEverythingSynced));

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

  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncAutofill),
            GetPrefs(1)->GetBoolean(prefs::kSyncAutofill));
  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncBookmarks),
            GetPrefs(1)->GetBoolean(prefs::kSyncBookmarks));
  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncExtensions),
            GetPrefs(1)->GetBoolean(prefs::kSyncExtensions));
  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));
}

// TCM ID - 3666296.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kShowBookmarkBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kShowBookmarkBar));

  bool new_kShowBookmarkBar = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowBookmarkBar);
  GetVerifierPrefs()->SetBoolean(prefs::kShowBookmarkBar, new_kShowBookmarkBar);
  GetPrefs(0)->SetBoolean(prefs::kShowBookmarkBar, new_kShowBookmarkBar);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(0)->GetBoolean(prefs::kShowBookmarkBar));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kShowBookmarkBar));
}

// TCM ID - 3611311.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kCheckDefaultBrowser) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kCheckDefaultBrowser),
            GetPrefs(1)->GetBoolean(prefs::kCheckDefaultBrowser));

  bool new_kCheckDefaultBrowser = !GetVerifierPrefs()->GetBoolean(
      prefs::kCheckDefaultBrowser);
  GetVerifierPrefs()->SetBoolean(prefs::kShowBookmarkBar,
      new_kCheckDefaultBrowser);
  GetPrefs(0)->SetBoolean(prefs::kCheckDefaultBrowser,
      new_kCheckDefaultBrowser);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(0)->GetBoolean(prefs::kCheckDefaultBrowser));
  ASSERT_NE(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kCheckDefaultBrowser));
}

// TCM ID - 3628298.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kHomePage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));

  GetVerifierPrefs()->SetString(prefs::kHomePage, "http://news.google.com");
  GetPrefs(0)->SetString(prefs::kHomePage, "http://news.google.com");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetString(prefs::kHomePage),
            GetPrefs(0)->GetString(prefs::kHomePage));
  ASSERT_EQ(GetVerifierPrefs()->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}

// TCM ID - 7297269.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kShowHomeButton) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
}

// TCM ID - 3710285.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kEnableTranslate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));

  bool new_kEnableTranslate = !GetVerifierPrefs()->GetBoolean(
      prefs::kEnableTranslate);
  GetVerifierPrefs()->SetBoolean(prefs::kEnableTranslate, new_kEnableTranslate);
  GetPrefs(0)->SetBoolean(prefs::kEnableTranslate, new_kEnableTranslate);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(0)->GetBoolean(prefs::kEnableTranslate));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));
}

// TCM ID - 3664293.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kAutofillEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutofillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutofillEnabled));

  bool new_kAutofillEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kAutofillEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kAutofillEnabled, new_kAutofillEnabled);
  GetPrefs(0)->SetBoolean(prefs::kAutofillEnabled, new_kAutofillEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutofillEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAutofillEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutofillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutofillEnabled));
}

// TCM ID - 3632259.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kURLsToRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup),
      GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_TRUE(GetPrefs(0)->GetList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetList(prefs::kURLsToRestoreOnStartup)));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 0);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 4);
  {
    ListPrefUpdate update_0(GetPrefs(0), prefs::kURLsToRestoreOnStartup);
    ListPrefUpdate update_verifier(GetVerifierPrefs(),
                                   prefs::kURLsToRestoreOnStartup);
    ListValue* url_list_client = update_0.Get();
    ListValue* url_list_verifier = update_verifier.Get();
    url_list_verifier->
        Append(Value::CreateStringValue("http://www.google.com/"));
    url_list_verifier->
        Append(Value::CreateStringValue("http://www.flickr.com/"));
    url_list_client->
        Append(Value::CreateStringValue("http://www.google.com/"));
    url_list_client->
        Append(Value::CreateStringValue("http://www.flickr.com/"));
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  ASSERT_TRUE(GetVerifierPrefs()->
      GetList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(0)->GetList(prefs::kURLsToRestoreOnStartup)));
  ASSERT_TRUE(GetVerifierPrefs()->
      GetList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetList(prefs::kURLsToRestoreOnStartup)));
}

// TCM ID - 3684287.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 1);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));
}

// TCM ID - 3703314.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Privacy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSearchSuggestEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kNetworkPredictionEnabled),
            GetPrefs(1)->GetBoolean(prefs::kNetworkPredictionEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));

  bool new_kAlternateErrorPagesEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kAlternateErrorPagesEnabled);
  bool new_kSearchSuggestEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSearchSuggestEnabled);
  bool new_kNetworkPredictionEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kNetworkPredictionEnabled);
  bool new_kSafeBrowsingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSafeBrowsingEnabled);

  GetVerifierPrefs()->SetBoolean(prefs::kAlternateErrorPagesEnabled,
      new_kAlternateErrorPagesEnabled);
  GetPrefs(0)->SetBoolean(prefs::kAlternateErrorPagesEnabled,
      new_kAlternateErrorPagesEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSearchSuggestEnabled,
      new_kSearchSuggestEnabled);
  GetPrefs(0)->SetBoolean(prefs::kSearchSuggestEnabled,
      new_kSearchSuggestEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kNetworkPredictionEnabled,
      new_kNetworkPredictionEnabled);
  GetPrefs(0)->SetBoolean(prefs::kNetworkPredictionEnabled,
      new_kNetworkPredictionEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSearchSuggestEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSearchSuggestEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled),
            GetPrefs(0)->GetBoolean(prefs::kNetworkPredictionEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kNetworkPredictionEnabled),
            GetPrefs(1)->GetBoolean(prefs::kNetworkPredictionEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));
}

// TCM ID - 3649279.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, ClearData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteBrowsingHistory));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteDownloadHistory));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteCache),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCache));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCookies));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(1)->GetBoolean(prefs::kDeletePasswords));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteFormData),
            GetPrefs(1)->GetBoolean(prefs::kDeleteFormData));

  bool new_kDeleteBrowsingHistory = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeleteBrowsingHistory);
  bool new_kDeleteDownloadHistory = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeleteDownloadHistory);
  bool new_kDeleteCache = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeleteCache);
  bool new_kDeleteCookies = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeleteCookies);
  bool new_kDeletePasswords = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeletePasswords);
  bool new_kDeleteFormData = !GetVerifierPrefs()->GetBoolean(
      prefs::kDeleteFormData);

  GetVerifierPrefs()->SetBoolean(prefs::kDeleteBrowsingHistory,
      new_kDeleteBrowsingHistory);
  GetPrefs(0)->SetBoolean(prefs::kDeleteBrowsingHistory,
      new_kDeleteBrowsingHistory);
  GetVerifierPrefs()->SetBoolean(prefs::kDeleteDownloadHistory,
      new_kDeleteDownloadHistory);
  GetPrefs(0)->SetBoolean(prefs::kDeleteDownloadHistory,
      new_kDeleteDownloadHistory);
  GetVerifierPrefs()->SetBoolean(prefs::kDeleteCache, new_kDeleteCache);
  GetPrefs(0)->SetBoolean(prefs::kDeleteCache, new_kDeleteCache);
  GetVerifierPrefs()->SetBoolean(prefs::kDeleteCookies, new_kDeleteCookies);
  GetPrefs(0)->SetBoolean(prefs::kDeleteCookies, new_kDeleteCookies);
  GetVerifierPrefs()->SetBoolean(prefs::kDeletePasswords, new_kDeletePasswords);
  GetPrefs(0)->SetBoolean(prefs::kDeletePasswords, new_kDeletePasswords);
  GetVerifierPrefs()->SetBoolean(prefs::kDeleteFormData, new_kDeleteFormData);
  GetPrefs(0)->SetBoolean(prefs::kDeleteFormData, new_kDeleteFormData);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(0)->GetBoolean(prefs::kDeleteBrowsingHistory));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteBrowsingHistory));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(0)->GetBoolean(prefs::kDeleteDownloadHistory));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteDownloadHistory));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCache),
            GetPrefs(0)->GetBoolean(prefs::kDeleteCache));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCache),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCache));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(0)->GetBoolean(prefs::kDeleteCookies));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCookies));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(0)->GetBoolean(prefs::kDeletePasswords));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(1)->GetBoolean(prefs::kDeletePasswords));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteFormData),
            GetPrefs(0)->GetBoolean(prefs::kDeleteFormData));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteFormData),
            GetPrefs(1)->GetBoolean(prefs::kDeleteFormData));
}

// TCM ID - 3686300.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kWebKitUsesUniversalDetector) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(1)->GetBoolean(prefs::kWebKitUsesUniversalDetector));

  bool new_kWebKitUsesUniversalDetector = !GetVerifierPrefs()->GetBoolean(
      prefs::kWebKitUsesUniversalDetector);
  GetVerifierPrefs()->SetBoolean(prefs::kWebKitUsesUniversalDetector,
      new_kWebKitUsesUniversalDetector);
  GetPrefs(0)->SetBoolean(prefs::kWebKitUsesUniversalDetector,
      new_kWebKitUsesUniversalDetector);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(0)->GetBoolean(prefs::kWebKitUsesUniversalDetector));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(1)->GetBoolean(prefs::kWebKitUsesUniversalDetector));
}

// TCM ID - 3673298.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kDefaultCharset) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetString(prefs::kDefaultCharset),
            GetPrefs(1)->GetString(prefs::kDefaultCharset));

  GetVerifierPrefs()->SetString(prefs::kDefaultCharset, "Thai");
  GetPrefs(0)->SetString(prefs::kDefaultCharset, "Thai");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetString(prefs::kDefaultCharset),
            GetPrefs(0)->GetString(prefs::kDefaultCharset));
  ASSERT_EQ(GetVerifierPrefs()->GetString(prefs::kDefaultCharset),
            GetPrefs(1)->GetString(prefs::kDefaultCharset));
}

// TCM ID - 3653296.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kBlockThirdPartyCookies) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(1)->GetBoolean(prefs::kBlockThirdPartyCookies));

  bool new_kBlockThirdPartyCookies = !GetVerifierPrefs()->GetBoolean(
      prefs::kBlockThirdPartyCookies);
  GetVerifierPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
      new_kBlockThirdPartyCookies);
  GetPrefs(0)->SetBoolean(prefs::kBlockThirdPartyCookies,
      new_kBlockThirdPartyCookies);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(0)->GetBoolean(prefs::kBlockThirdPartyCookies));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(1)->GetBoolean(prefs::kBlockThirdPartyCookies));
}

// TCM ID - 7297279.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kClearSiteDataOnExit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(1)->GetBoolean(prefs::kClearSiteDataOnExit));

  bool new_kClearSiteDataOnExit = !GetVerifierPrefs()->GetBoolean(
      prefs::kClearSiteDataOnExit);
  GetVerifierPrefs()->SetBoolean(prefs::kClearSiteDataOnExit,
      new_kClearSiteDataOnExit);
  GetPrefs(0)->SetBoolean(prefs::kClearSiteDataOnExit,
      new_kClearSiteDataOnExit);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(0)->GetBoolean(prefs::kClearSiteDataOnExit));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(1)->GetBoolean(prefs::kClearSiteDataOnExit));
}

// TCM ID - 7306184.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kSafeBrowsingEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));

  bool new_kSafeBrowsingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSafeBrowsingEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));
}

// TCM ID - 3624302.
// The kAutofillAuxiliaryProfilesEnabled preference key is currently only
// synced on Mac and not on Windows or Linux.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kAutofillAuxiliaryProfilesEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));

  GetVerifierPrefs()->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, 0);
  GetPrefs(0)->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

#if defined(OS_MACOSX)
  ASSERT_NE(GetVerifierPrefs()->GetBoolean(
                                prefs::kAutofillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(
                         prefs::kAutofillAuxiliaryProfilesEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
#else
  ASSERT_NE(GetPrefs(1)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
#endif  // OS_MACOSX
}

// TCM ID - 3717298.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kPromptForDownload) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPromptForDownload),
            GetPrefs(1)->GetBoolean(prefs::kPromptForDownload));

  bool new_kPromptForDownload = !GetVerifierPrefs()->GetBoolean(
      prefs::kPromptForDownload);
  GetVerifierPrefs()->SetBoolean(prefs::kPromptForDownload,
      new_kPromptForDownload);
  GetPrefs(0)->SetBoolean(prefs::kPromptForDownload,
      new_kPromptForDownload);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPromptForDownload),
            GetPrefs(0)->GetBoolean(prefs::kPromptForDownload));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPromptForDownload),
            GetPrefs(1)->GetBoolean(prefs::kPromptForDownload));
}

// TCM ID - 3729263.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kPrefTranslateLanguageBlacklist) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));

  TranslatePrefs translate_client0_prefs(GetPrefs(0));
  TranslatePrefs translate_client1_prefs(GetPrefs(1));
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
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kPrefTranslateWhitelists) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));

  TranslatePrefs translate_client0_prefs(GetPrefs(0));
  TranslatePrefs translate_client1_prefs(GetPrefs(1));
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
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kPrefTranslateSiteBlacklist) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));

  GURL url("http://www.google.com");
  std::string host(url.host());
  TranslatePrefs translate_client0_prefs(GetPrefs(0));
  TranslatePrefs translate_client1_prefs(GetPrefs(1));
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
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kExtensionsUIDeveloperMode) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kExtensionsUIDeveloperMode),
            GetPrefs(1)->GetBoolean(prefs::kExtensionsUIDeveloperMode));

  bool new_kExtensionsUIDeveloperMode = !GetPrefs(0)->GetBoolean(
      prefs::kExtensionsUIDeveloperMode);
  GetVerifierPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
      new_kExtensionsUIDeveloperMode);
  GetPrefs(0)->SetBoolean(prefs::kExtensionsUIDeveloperMode,
      new_kExtensionsUIDeveloperMode);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode),
            GetPrefs(0)->GetBoolean(prefs::kExtensionsUIDeveloperMode));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode),
            GetPrefs(1)->GetBoolean(prefs::kExtensionsUIDeveloperMode));
}

// TCM ID - 6473347.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kTapToClickEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kTapToClickEnabled),
            GetPrefs(1)->GetBoolean(prefs::kTapToClickEnabled));

  bool new_kTapToClickEnabled = !GetPrefs(0)->GetBoolean(
      prefs::kTapToClickEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kTapToClickEnabled,
      new_kTapToClickEnabled);
  GetPrefs(0)->SetBoolean(prefs::kTapToClickEnabled, new_kTapToClickEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kTapToClickEnabled),
            GetPrefs(1)->GetBoolean(prefs::kTapToClickEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kTapToClickEnabled),
            GetPrefs(1)->GetBoolean(prefs::kTapToClickEnabled));

  GetVerifierPrefs()->SetBoolean(prefs::kTapToClickEnabled,
      !new_kTapToClickEnabled);
  GetPrefs(0)->SetBoolean(prefs::kTapToClickEnabled,
      !new_kTapToClickEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kTapToClickEnabled),
            GetPrefs(1)->GetBoolean(prefs::kTapToClickEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kTapToClickEnabled),
            GetPrefs(1)->GetBoolean(prefs::kTapToClickEnabled));
}
#endif  // OS_CHROMEOS

// TCM ID - 6458824.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kEnableScreenLock) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableScreenLock),
            GetPrefs(1)->GetBoolean(prefs::kEnableScreenLock));

  bool new_kEnableScreenLock = !GetPrefs(0)->GetBoolean(
      prefs::kEnableScreenLock);
  GetVerifierPrefs()->SetBoolean(prefs::kEnableScreenLock,
      new_kEnableScreenLock);
  GetPrefs(0)->SetBoolean(prefs::kEnableScreenLock, new_kEnableScreenLock);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableScreenLock),
            GetPrefs(1)->GetBoolean(prefs::kEnableScreenLock));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableScreenLock),
            GetPrefs(1)->GetBoolean(prefs::kEnableScreenLock));

  GetVerifierPrefs()->SetBoolean(prefs::kEnableScreenLock,
      !new_kEnableScreenLock);
  GetPrefs(0)->SetBoolean(prefs::kEnableScreenLock,
      !new_kEnableScreenLock);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableScreenLock),
            GetPrefs(1)->GetBoolean(prefs::kEnableScreenLock));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableScreenLock),
            GetPrefs(1)->GetBoolean(prefs::kEnableScreenLock));
}
#endif  // OS_CHROMEOS
