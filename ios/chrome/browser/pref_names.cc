// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/pref_names.h"

namespace ios {
namespace prefs {

// Preferences in ios::prefs:: must have the same value as the corresponding
// preference on desktop.
// See chrome/common/pref_names.cc for a detailed description of each
// preference.

const char kAcceptLanguages[] = "intl.accept_languages";
const char kBrowserStateInfoCache[] = "profile.info_cache";
const char kBrowserStateLastUsed[] = "profile.last_used";
const char kBrowserStatesLastActive[] = "profile.last_active_profiles";
const char kBrowserStatesNumCreated[] = "profile.profiles_created";
const char kDefaultCharset[] = "intl.charset_default";
const char kEnableDoNotTrack[] = "enable_do_not_track";
const char kHttpServerProperties[] = "net.http_server_properties";
const char kMaxConnectionsPerProxy[] = "net.max_connections_per_proxy";
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

// Boolean which indicates whether browsing data migration is/was possible in
// this or a previous cold start.
const char kBrowsingDataMigrationHasBeenPossible[] =
    "ios.browsing_data_migration_controller.migration_has_been_possible";

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

// A boolean pref set to true if prediction of network actions is allowed.
// Actions include prerendering of web pages.
// NOTE: The "dns_prefetching.enabled" value is used so that historical user
// preferences are not lost.
const char kNetworkPredictionEnabled[] = "dns_prefetching.enabled";

// Preference that hold a boolean indicating whether network prediction should
// be limited to wifi (when enabled).
const char kNetworkPredictionWifiOnly[] = "ios.dns_prefetching.wifi_only";

// Which bookmarks folder should be visible on the new tab page v4.
const char kNtpShownBookmarksFolder[] = "ntp.shown_bookmarks_folder";

// True if the memory debugging tools should be visible.
const char kShowMemoryDebuggingTools[] = "ios.memory.show_debugging_tools";

// User preferred speech input language for voice search.
const char kVoiceSearchLocale[] = "ios.speechinput.voicesearch_locale";

// Boolean which indicates if TTS after voice search is enabled.
const char kVoiceSearchTTS[] = "ios.speechinput.voicesearch_tts";

// List which contains the last known list of accounts.
const char kSigninLastAccounts[] = "ios.signin.last_accounts";

// Boolean which indicates if the pref which contains the last known list of
// accounts was migrated to use account ids instead of emails.
const char kSigninLastAccountsMigrated[] = "ios.signin.last_accounts_migrated";

// String which contains the user id of the user signed in with shared
// authentication.
const char kSigninSharedAuthenticationUserId[] =
    "ios.signin.shared_authentication_user_id";

// Boolean which indicates if user should be prompted to sign in again
// when a new tab is created.
const char kSigninShouldPromptForSigninAgain[] =
    "ios.signin.should_prompt_for_signin_again";

// Integer which indicates whether the user has authorized using geolocation
// for Omnibox queries or the progress towards soliciting the user's
// authorization.
const char kOmniboxGeolocationAuthorizationState[] =
    "ios.omnibox.geolocation_authorization_state";

// String which contains the application version when we last showed the
// authorization alert.
const char kOmniboxGeolocationLastAuthorizationAlertVersion[] =
    "ios.omnibox.geolocation_last_authorization_alert_version";

// Integer which contains the timestamp at which the "Rate This App" dialog was
// last shown.
const char kRateThisAppDialogLastShownTime[] =
    "ios.ratethisapp.dialog_last_shown_time";

}  // namespace prefs
