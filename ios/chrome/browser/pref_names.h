// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREF_NAMES_H_

namespace ios {
namespace prefs {

// Preferences in ios::prefs:: are temporary shared with desktop Chrome.
// Non-shared preferences should be in the prefs:: namespace (no ios::).
extern const char kAcceptLanguages[];
extern const char kHomePage[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kSearchSuggestEnabled[];

}  // namespace prefs
}  // namespace ios

namespace prefs {

extern const char kContextualSearchEnabled[];
extern const char kIosBookmarkFolderDefault[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kOTRStashStatePathSystemBackupExcluded[];
extern const char kIosHandoffToOtherDevices[];
extern const char kLastSessionExitedCleanly[];
extern const char kMetricsReportingWifiOnly[];
extern const char kNetworkPredictionWifiOnly[];
extern const char kNtpShownBookmarksFolder[];
extern const char kShowMemoryDebuggingTools[];

extern const char kVoiceSearchLocale[];
extern const char kVoiceSearchTTS[];

extern const char kSigninLastAccounts[];
extern const char kSigninSharedAuthenticationUserId[];
extern const char kSigninShouldPromptForSigninAgain[];

extern const char kOmniboxGeolocationAuthorizationState[];
extern const char kOmniboxGeolocationLastAuthorizationAlertVersion[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREF_NAMES_H_
