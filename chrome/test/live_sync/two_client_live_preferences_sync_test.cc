// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

// TestScribe ID - 423959 (kHomePageIsNewTabPage).
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

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetPrefs(0)->SetString(prefs::kHomePage, "http://www.google.com/1");
  GetPrefs(1)->SetString(prefs::kHomePage, "http://www.google.com/2");
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}

// TestScribe ID - 425635.
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

// TestScribe ID - 427426.
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

// TestScribe ID - 426093.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kSyncPreferences) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  GetClient(0)->DisableSyncForDatatype(syncable::PREFERENCES);

  GetPrefs(0)->SetBoolean(prefs::kPasswordManagerEnabled, 1);
  GetPrefs(1)->SetBoolean(prefs::kPasswordManagerEnabled, 0);
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_NE(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));
}

// TestScribe ID - 425647.
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

// TestScribe ID - 423960.
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

// TestScribe ID - 423962.
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

// TestScribe ID - 423959 (kHomePage).
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

// TestScribe ID - 423959 (kShowHomeButton).
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

// TestScribe ID - 425641.
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

// TestScribe ID - 425648.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kAutoFillEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillEnabled));

  bool new_kAutoFillEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kAutoFillEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kAutoFillEnabled, new_kAutoFillEnabled);
  GetPrefs(0)->SetBoolean(prefs::kAutoFillEnabled, new_kAutoFillEnabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAutoFillEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillEnabled));
}

// TestScribe ID - 425666.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kURLsToRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup),
      GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_TRUE(GetPrefs(0)->GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetMutableList(prefs::kURLsToRestoreOnStartup)));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 0);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 4);
  ListValue* url_list_verifier = GetVerifierPrefs()->
      GetMutableList(prefs::kURLsToRestoreOnStartup);
  ListValue* url_list_client = GetPrefs(0)->
      GetMutableList(prefs::kURLsToRestoreOnStartup);
  {
    url_list_verifier->
        Append(Value::CreateStringValue("http://www.google.com/"));
    url_list_verifier->
        Append(Value::CreateStringValue("http://www.flickr.com/"));
    url_list_client->
        Append(Value::CreateStringValue("http://www.google.com/"));
    url_list_client->
        Append(Value::CreateStringValue("http://www.flickr.com/"));
    ScopedUserPrefUpdate update(GetPrefs(0), prefs::kURLsToRestoreOnStartup);
  }

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  ASSERT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  ASSERT_TRUE(GetVerifierPrefs()->
      GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(0)->GetMutableList(prefs::kURLsToRestoreOnStartup)));
  ASSERT_TRUE(GetVerifierPrefs()->
      GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetMutableList(prefs::kURLsToRestoreOnStartup)));
}

// TestScribe ID - 423958.
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

// TestScribe ID - 425644.
#if defined(USE_NSS)
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Security) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kCertRevocationCheckingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL3Enabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(1)->GetBoolean(prefs::kTLS1Enabled));

  bool new_kCertRevocationCheckingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kCertRevocationCheckingEnabled);
  bool new_kSSL3Enabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSSL3Enabled);
  bool new_kTLS1Enabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kTLS1Enabled);

  GetVerifierPrefs()->SetBoolean(prefs::kCertRevocationCheckingEnabled,
      new_kCertRevocationCheckingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kCertRevocationCheckingEnabled,
      new_kCertRevocationCheckingEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSSL3Enabled, new_kSSL3Enabled);
  GetPrefs(0)->SetBoolean(prefs::kSSL3Enabled, new_kSSL3Enabled);
  GetVerifierPrefs()->SetBoolean(prefs::kTLS1Enabled, new_kTLS1Enabled);
  GetPrefs(0)->SetBoolean(prefs::kTLS1Enabled, new_kTLS1Enabled);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->
      GetBoolean(prefs::kCertRevocationCheckingEnabled),
      GetPrefs(0)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  ASSERT_NE(GetVerifierPrefs()->
      GetBoolean(prefs::kCertRevocationCheckingEnabled),
      GetPrefs(1)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(0)->GetBoolean(prefs::kSSL3Enabled));
  ASSERT_NE(GetVerifierPrefs()->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL3Enabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(0)->GetBoolean(prefs::kTLS1Enabled));
  ASSERT_NE(GetVerifierPrefs()->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(1)->GetBoolean(prefs::kTLS1Enabled));
}
#endif  // USE_NSS

// TestScribe ID - 425639.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Privacy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSearchSuggestEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));

  bool new_kAlternateErrorPagesEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kAlternateErrorPagesEnabled);
  bool new_kSearchSuggestEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSearchSuggestEnabled);
  bool new_kDnsPrefetchingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kDnsPrefetchingEnabled);
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
  GetVerifierPrefs()->SetBoolean(prefs::kDnsPrefetchingEnabled,
      new_kDnsPrefetchingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kDnsPrefetchingEnabled,
      new_kDnsPrefetchingEnabled);
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
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));
}

// TestScribe ID - 426766.
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

// TestScribe ID - 425903.
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

// TestScribe ID - 425643.
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

// TestScribe ID - 425675 (kBlockThirdPartyCookies).
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

// TestScribe ID - 425675 (kClearSiteDataOnExit).
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

// TestScribe ID - 425639.
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

// TestScribe ID - 433525.
// The kAutoFillAuxiliaryProfilesEnabled preference key is currently only
// synced on Mac and not on Windows or Linux.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kAutoFillAuxiliaryProfilesEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled));

  GetVerifierPrefs()->SetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled, 0);
  GetPrefs(0)->SetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

#if defined(OS_MACOSX)
  ASSERT_NE(GetVerifierPrefs()->GetBoolean(
                                prefs::kAutoFillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(
                         prefs::kAutoFillAuxiliaryProfilesEnabled));
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled));
#else
  ASSERT_NE(GetPrefs(1)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAutoFillAuxiliaryProfilesEnabled));
#endif  // OS_MACOSX
}

// TestScribe ID - 433564.
// TODO(annapop): Enable after crbug.com/71510 is fixed.
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       DISABLED_kCookiePromptExpanded) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_EQ(GetPrefs(0)->GetBoolean(prefs::kCookiePromptExpanded),
            GetPrefs(1)->GetBoolean(prefs::kCookiePromptExpanded));

  bool new_kCookiePromptExpanded = !GetPrefs(0)->GetBoolean(
      prefs::kCookiePromptExpanded);
  GetVerifierPrefs()->SetBoolean(prefs::kCookiePromptExpanded,
      new_kCookiePromptExpanded);
  GetPrefs(0)->SetBoolean(prefs::kCookiePromptExpanded,
      new_kCookiePromptExpanded);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kCookiePromptExpanded),
            GetPrefs(0)->GetBoolean(prefs::kCookiePromptExpanded));
  ASSERT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kCookiePromptExpanded),
            GetPrefs(1)->GetBoolean(prefs::kCookiePromptExpanded));
}

// TestScribe ID - 425642.
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

// TestScribe ID - 426767 (kPrefTranslateLanguageBlacklist).
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

// TestScribe ID - 426767 (kPrefTranslateWhitelists).
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

// TestScribe ID - 426768.
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
