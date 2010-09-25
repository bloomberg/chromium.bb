// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/scoped_pref_update.h"
#include "chrome/test/live_sync/live_preferences_sync_test.h"

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kHomePageIsNewTabPage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));

  bool new_kHomePageIsNewTabPage = !GetVerifierPrefs()->GetBoolean(
      prefs::kHomePageIsNewTabPage);
  GetVerifierPrefs()->SetBoolean(prefs::kHomePageIsNewTabPage,
      new_kHomePageIsNewTabPage);
  GetPrefs(0)->SetBoolean(prefs::kHomePageIsNewTabPage,
      new_kHomePageIsNewTabPage);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(0)->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage),
            GetPrefs(1)->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Race) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetPrefs(0)->SetString(prefs::kHomePage, "http://www.google.com/1");
  GetPrefs(1)->SetString(prefs::kHomePage, "http://www.google.com/2");
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kPasswordManagerEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  bool new_kPasswordManagerEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kPasswordManagerEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  GetPrefs(0)->SetBoolean(prefs::kPasswordManagerEnabled,
      new_kPasswordManagerEnabled);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kKeepEverythingSynced) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
            GetPrefs(1)->GetBoolean(prefs::kKeepEverythingSynced));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));

  GetClient(0)->DisableSyncForDatatype(syncable::THEMES);

  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
            GetPrefs(1)->GetBoolean(prefs::kKeepEverythingSynced));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kSyncPreferences) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));

  GetClient(0)->DisableSyncForDatatype(syncable::PREFERENCES);

  GetPrefs(0)->SetBoolean(prefs::kPasswordManagerEnabled, 1);
  GetPrefs(1)->SetBoolean(prefs::kPasswordManagerEnabled, 0);
  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kPasswordManagerEnabled),
            GetPrefs(1)->GetBoolean(prefs::kPasswordManagerEnabled));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, SignInDialog) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncBookmarks),
            GetPrefs(1)->GetBoolean(prefs::kSyncBookmarks));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncExtensions),
            GetPrefs(1)->GetBoolean(prefs::kSyncExtensions));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSyncAutofill),
            GetPrefs(1)->GetBoolean(prefs::kSyncAutofill));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kKeepEverythingSynced),
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

  ASSERT_TRUE(ProfileSyncServiceTestHarness::AwaitQuiescence(clients()));

  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncPreferences),
            GetPrefs(1)->GetBoolean(prefs::kSyncPreferences));
  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncAutofill),
            GetPrefs(1)->GetBoolean(prefs::kSyncAutofill));
  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncBookmarks),
            GetPrefs(1)->GetBoolean(prefs::kSyncBookmarks));
  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncExtensions),
            GetPrefs(1)->GetBoolean(prefs::kSyncExtensions));
  EXPECT_NE(GetPrefs(0)->GetBoolean(prefs::kSyncThemes),
            GetPrefs(1)->GetBoolean(prefs::kSyncThemes));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kShowBookmarkBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kShowBookmarkBar));

  bool new_kShowBookmarkBar = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowBookmarkBar);
  GetVerifierPrefs()->SetBoolean(prefs::kShowBookmarkBar, new_kShowBookmarkBar);
  GetPrefs(0)->SetBoolean(prefs::kShowBookmarkBar, new_kShowBookmarkBar);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(0)->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kShowBookmarkBar));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kCheckDefaultBrowser) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kCheckDefaultBrowser),
            GetPrefs(1)->GetBoolean(prefs::kCheckDefaultBrowser));

  bool new_kCheckDefaultBrowser = !GetVerifierPrefs()->GetBoolean(
      prefs::kCheckDefaultBrowser);
  GetVerifierPrefs()->SetBoolean(prefs::kShowBookmarkBar,
      new_kCheckDefaultBrowser);
  GetPrefs(0)->SetBoolean(prefs::kCheckDefaultBrowser,
      new_kCheckDefaultBrowser);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(0)->GetBoolean(prefs::kCheckDefaultBrowser));
  EXPECT_NE(GetVerifierPrefs()->GetBoolean(prefs::kShowBookmarkBar),
            GetPrefs(1)->GetBoolean(prefs::kCheckDefaultBrowser));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kHomePage) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));

  GetVerifierPrefs()->SetString(prefs::kHomePage, "http://news.google.com");
  GetPrefs(0)->SetString(prefs::kHomePage, "http://news.google.com");
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetString(prefs::kHomePage),
            GetPrefs(0)->GetString(prefs::kHomePage));
  EXPECT_EQ(GetVerifierPrefs()->GetString(prefs::kHomePage),
            GetPrefs(1)->GetString(prefs::kHomePage));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kShowHomeButton) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));

  bool new_kShowHomeButton = !GetVerifierPrefs()->GetBoolean(
      prefs::kShowHomeButton);
  GetVerifierPrefs()->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  GetPrefs(0)->SetBoolean(prefs::kShowHomeButton, new_kShowHomeButton);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(0)->GetBoolean(prefs::kShowHomeButton));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kShowHomeButton),
            GetPrefs(1)->GetBoolean(prefs::kShowHomeButton));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kEnableTranslate) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));

  bool new_kEnableTranslate = !GetVerifierPrefs()->GetBoolean(
      prefs::kEnableTranslate);
  GetVerifierPrefs()->SetBoolean(prefs::kEnableTranslate, new_kEnableTranslate);
  GetPrefs(0)->SetBoolean(prefs::kEnableTranslate, new_kEnableTranslate);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(0)->GetBoolean(prefs::kEnableTranslate));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kEnableTranslate),
            GetPrefs(1)->GetBoolean(prefs::kEnableTranslate));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kAutoFillEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillEnabled));

  bool new_kAutoFillEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kAutoFillEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kAutoFillEnabled, new_kAutoFillEnabled);
  GetPrefs(0)->SetBoolean(prefs::kAutoFillEnabled, new_kAutoFillEnabled);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAutoFillEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAutoFillEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAutoFillEnabled));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kURLsToRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup),
      GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));
  EXPECT_TRUE(GetPrefs(0)->GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetMutableList(prefs::kURLsToRestoreOnStartup)));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 0);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 0);

  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
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
    ScopedPrefUpdate update(GetPrefs(0), prefs::kURLsToRestoreOnStartup);
  }

  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  EXPECT_TRUE(GetVerifierPrefs()->
      GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(0)->GetMutableList(prefs::kURLsToRestoreOnStartup)));
  EXPECT_TRUE(GetVerifierPrefs()->
      GetMutableList(prefs::kURLsToRestoreOnStartup)->
      Equals(GetPrefs(1)->GetMutableList(prefs::kURLsToRestoreOnStartup)));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kRestoreOnStartup) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));

  GetVerifierPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  GetPrefs(0)->SetInteger(prefs::kRestoreOnStartup, 1);

  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(0)->GetInteger(prefs::kRestoreOnStartup));
  EXPECT_EQ(GetVerifierPrefs()->GetInteger(prefs::kRestoreOnStartup),
            GetPrefs(1)->GetInteger(prefs::kRestoreOnStartup));
}

#if defined(USE_NSS)
IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Security) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kCertRevocationCheckingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSSL2Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL2Enabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL3Enabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(1)->GetBoolean(prefs::kTLS1Enabled));

  bool new_kCertRevocationCheckingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kCertRevocationCheckingEnabled);
  bool new_kSSL2Enabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSSL2Enabled);
  bool new_kSSL3Enabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSSL3Enabled);
  bool new_kTLS1Enabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kTLS1Enabled);

  GetVerifierPrefs()->SetBoolean(prefs::kCertRevocationCheckingEnabled,
      new_kCertRevocationCheckingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kCertRevocationCheckingEnabled,
      new_kCertRevocationCheckingEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSSL2Enabled, new_kSSL2Enabled);
  GetPrefs(0)->SetBoolean(prefs::kSSL2Enabled, new_kSSL2Enabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSSL3Enabled, new_kSSL3Enabled);
  GetPrefs(0)->SetBoolean(prefs::kSSL3Enabled, new_kSSL3Enabled);
  GetVerifierPrefs()->SetBoolean(prefs::kTLS1Enabled, new_kTLS1Enabled);
  GetPrefs(0)->SetBoolean(prefs::kTLS1Enabled, new_kTLS1Enabled);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->
      GetBoolean(prefs::kCertRevocationCheckingEnabled),
      GetPrefs(0)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  EXPECT_NE(GetVerifierPrefs()->
      GetBoolean(prefs::kCertRevocationCheckingEnabled),
      GetPrefs(1)->GetBoolean(prefs::kCertRevocationCheckingEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSSL2Enabled),
            GetPrefs(0)->GetBoolean(prefs::kSSL2Enabled));
  EXPECT_NE(GetVerifierPrefs()->GetBoolean(prefs::kSSL2Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL2Enabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(0)->GetBoolean(prefs::kSSL3Enabled));
  EXPECT_NE(GetVerifierPrefs()->GetBoolean(prefs::kSSL3Enabled),
            GetPrefs(1)->GetBoolean(prefs::kSSL3Enabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(0)->GetBoolean(prefs::kTLS1Enabled));
  EXPECT_NE(GetVerifierPrefs()->GetBoolean(prefs::kTLS1Enabled),
            GetPrefs(1)->GetBoolean(prefs::kTLS1Enabled));
}
#endif  // USE_NSS

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, Privacy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSearchSuggestEnabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled),
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
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(0)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kAlternateErrorPagesEnabled),
            GetPrefs(1)->GetBoolean(prefs::kAlternateErrorPagesEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSearchSuggestEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSearchSuggestEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSearchSuggestEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDnsPrefetchingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kDnsPrefetchingEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, ClearData) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteBrowsingHistory));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteDownloadHistory));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteCache),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCache));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCookies));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(1)->GetBoolean(prefs::kDeletePasswords));
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kDeleteFormData),
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
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(0)->GetBoolean(prefs::kDeleteBrowsingHistory));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteBrowsingHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteBrowsingHistory));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(0)->GetBoolean(prefs::kDeleteDownloadHistory));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteDownloadHistory),
            GetPrefs(1)->GetBoolean(prefs::kDeleteDownloadHistory));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCache),
            GetPrefs(0)->GetBoolean(prefs::kDeleteCache));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCache),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCache));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(0)->GetBoolean(prefs::kDeleteCookies));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteCookies),
            GetPrefs(1)->GetBoolean(prefs::kDeleteCookies));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(0)->GetBoolean(prefs::kDeletePasswords));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeletePasswords),
            GetPrefs(1)->GetBoolean(prefs::kDeletePasswords));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteFormData),
            GetPrefs(0)->GetBoolean(prefs::kDeleteFormData));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kDeleteFormData),
            GetPrefs(1)->GetBoolean(prefs::kDeleteFormData));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kWebKitUsesUniversalDetector) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(1)->GetBoolean(prefs::kWebKitUsesUniversalDetector));

  bool new_kWebKitUsesUniversalDetector = !GetVerifierPrefs()->GetBoolean(
      prefs::kWebKitUsesUniversalDetector);
  GetVerifierPrefs()->SetBoolean(prefs::kWebKitUsesUniversalDetector,
      new_kWebKitUsesUniversalDetector);
  GetPrefs(0)->SetBoolean(prefs::kWebKitUsesUniversalDetector,
      new_kWebKitUsesUniversalDetector);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(0)->GetBoolean(prefs::kWebKitUsesUniversalDetector));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kWebKitUsesUniversalDetector),
            GetPrefs(1)->GetBoolean(prefs::kWebKitUsesUniversalDetector));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest, kDefaultCharset) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetString(prefs::kDefaultCharset),
            GetPrefs(1)->GetString(prefs::kDefaultCharset));

  GetVerifierPrefs()->SetString(prefs::kDefaultCharset, "Thai");
  GetPrefs(0)->SetString(prefs::kDefaultCharset, "Thai");
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetString(prefs::kDefaultCharset),
            GetPrefs(0)->GetString(prefs::kDefaultCharset));
  EXPECT_EQ(GetVerifierPrefs()->GetString(prefs::kDefaultCharset),
            GetPrefs(1)->GetString(prefs::kDefaultCharset));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kBlockThirdPartyCookies) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(1)->GetBoolean(prefs::kBlockThirdPartyCookies));

  bool new_kBlockThirdPartyCookies = !GetVerifierPrefs()->GetBoolean(
      prefs::kBlockThirdPartyCookies);
  GetVerifierPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies,
      new_kBlockThirdPartyCookies);
  GetPrefs(0)->SetBoolean(prefs::kBlockThirdPartyCookies,
      new_kBlockThirdPartyCookies);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(0)->GetBoolean(prefs::kBlockThirdPartyCookies));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies),
            GetPrefs(1)->GetBoolean(prefs::kBlockThirdPartyCookies));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kClearSiteDataOnExit) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(1)->GetBoolean(prefs::kClearSiteDataOnExit));

  bool new_kClearSiteDataOnExit = !GetVerifierPrefs()->GetBoolean(
      prefs::kClearSiteDataOnExit);
  GetVerifierPrefs()->SetBoolean(prefs::kClearSiteDataOnExit,
      new_kClearSiteDataOnExit);
  GetPrefs(0)->SetBoolean(prefs::kClearSiteDataOnExit,
      new_kClearSiteDataOnExit);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(0)->GetBoolean(prefs::kClearSiteDataOnExit));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kClearSiteDataOnExit),
            GetPrefs(1)->GetBoolean(prefs::kClearSiteDataOnExit));
}

IN_PROC_BROWSER_TEST_F(TwoClientLivePreferencesSyncTest,
                       kSafeBrowsingEnabled) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  EXPECT_EQ(GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));

  bool new_kSafeBrowsingEnabled = !GetVerifierPrefs()->GetBoolean(
      prefs::kSafeBrowsingEnabled);
  GetVerifierPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  GetPrefs(0)->SetBoolean(prefs::kSafeBrowsingEnabled,
      new_kSafeBrowsingEnabled);
  EXPECT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(0)->GetBoolean(prefs::kSafeBrowsingEnabled));
  EXPECT_EQ(GetVerifierPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled),
            GetPrefs(1)->GetBoolean(prefs::kSafeBrowsingEnabled));
}
