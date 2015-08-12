// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/pref_names.h"

namespace ios {
namespace prefs {

// Preferences in ios::prefs:: must have the same value as the corresponding
// preference on desktop.
// These preferences must not be registered by //ios code.
// See chrome/common/pref_names.cc for a detailed description of each
// preference.

const char kAcceptLanguages[] = "intl.accept_languages";
const char kHomePage[] = "homepage";
const char kSavingBrowserHistoryDisabled[] = "history.saving_disabled";
const char kSearchSuggestEnabled[] = "search.suggest_enabled";

}  // namespace prefs
}  // namespace ios

namespace prefs {

// *************** CHROME BROWSER STATE PREFS ***************
// These are attached to the user Chrome browser state.

// String indicating the Contextual Search enabled state.
// "false" - opt-out (disabled)
// "" (empty string) - undecided
// "true" - opt-in (enabled)
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";

// Preference that keep information about where to create a new bookmark.
const char kIosBookmarkFolderDefault[] = "ios.bookmark.default_folder";

// Preference that hold a boolean indicating if the user has already dismissed
// the bookmark promo dialog.
const char kIosBookmarkPromoAlreadySeen[] = "ios.bookmark.promo_already_seen";

// Boolean which indicates if the user has already set a "do not backup" bit to
// the OTR Profiles's state stash path to ensure that the folder is not
// automatically synced to iCloud/iTunes.
const char kOTRStashStatePathSystemBackupExcluded[] =
    "ios.otr_stash_state_path_system_backup_excluded";

// Whether Chrome should attempt to hand off the current URL to other Apple
// devices that share an iCloud account.
const char kIosHandoffToOtherDevices[] = "ios.handoff_to_other_devices";

// True if the previous session exited cleanly.
// This can be different from kStabilityExitedCleanly, because the last run of
// the program may not have included a browsing session, and thus the last run
// of the program may have happened after the run that included the last
// session.
const char kLastSessionExitedCleanly[] =
    "ios.user_experience_metrics.last_session_exited_cleanly";

// Preference that hold a boolean indicating whether metrics reporting should
// be limited to wifi (when enabled).
const char kMetricsReportingWifiOnly[] =
    "ios.user_experience_metrics.wifi_only";

// Preference that hold a boolean indicating whether network prediction should
// be limited to wifi (when enabled).
const char kNetworkPredictionWifiOnly[] = "ios.dns_prefetching.wifi_only";

// Which bookmarks folder should be visible on the new tab page v4.
const char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";

// True if the memory debugging tools should be visible.
extern const char kShowMemoryDebuggingTools[] =
    "ios.memory.show_debugging_tools";

// User preferred speech input language for voice search.
const char kVoiceSearchLocale[] = "ios.speechinput.voicesearch_locale";

// Boolean which indicates if TTS after voice search is enabled.
extern const char kVoiceSearchTTS[] = "ios.speechinput.voicesearch_tts";

// List which contains the last known list of accounts.
extern const char kSigninLastAccounts[] = "ios.signin.last_accounts";

// String which contains the user id of the user signed in with shared
// authentication.
extern const char kSigninSharedAuthenticationUserId[] =
    "ios.signin.shared_authentication_user_id";

// Boolean which indicates if user should be prompted to sign in again
// when a new tab is created.
extern const char kSigninShouldPromptForSigninAgain[] =
    "ios.signin.should_prompt_for_signin_again";

// Integer which indicates whether the user has authorized using geolocation
// for Omnibox queries or the progress towards soliciting the user's
// authorization.
extern const char kOmniboxGeolocationAuthorizationState[] =
    "ios.omnibox.geolocation_authorization_state";

// String which contains the application version when we last showed the
// authorization alert.
extern const char kOmniboxGeolocationLastAuthorizationAlertVersion[] =
    "ios.omnibox.geolocation_last_authorization_alert_version";

}  // namespace prefs
