// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/pref_names.h"

namespace sync_driver {

namespace prefs {

// Set to true when enhanced bookmarks experiment is enabled via Chrome sync.
const char kEnhancedBookmarksExperimentEnabled[] = "enhanced_bookmarks_enabled";

// Enhanced bookmarks extension id passed via Chrome sync.
const char kEnhancedBookmarksExtensionId[] = "enhanced_bookmarks_extension_id";

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// Boolean specifying whether the user finished setting up sync.
const char kSyncHasSetupCompleted[] = "sync.has_setup_completed";

// Boolean specifying whether sync has an auth error.
const char kSyncHasAuthError[] = "sync.has_auth_error";

// Boolean specifying whether to automatically sync all data types (including
// future ones, as they're added).  If this is true, the following preferences
// (kSyncBookmarks, kSyncPasswords, etc.) can all be ignored.
const char kSyncKeepEverythingSynced[] = "sync.keep_everything_synced";

// Booleans specifying whether the user has selected to sync the following
// datatypes.
const char kSyncAppList[] = "sync.app_list";
const char kSyncAppNotifications[] = "sync.app_notifications";
const char kSyncAppSettings[] = "sync.app_settings";
const char kSyncApps[] = "sync.apps";
const char kSyncArticles[] = "sync.articles";
const char kSyncAutofillProfile[] = "sync.autofill_profile";
const char kSyncAutofill[] = "sync.autofill";
const char kSyncBookmarks[] = "sync.bookmarks";
const char kSyncDictionary[] = "sync.dictionary";
const char kSyncExtensionSettings[] = "sync.extension_settings";
const char kSyncExtensions[] = "sync.extensions";
const char kSyncFaviconImages[] = "sync.favicon_images";
const char kSyncFaviconTracking[] = "sync.favicon_tracking";
const char kSyncHistoryDeleteDirectives[] = "sync.history_delete_directives";
const char kSyncPasswords[] = "sync.passwords";
const char kSyncPreferences[] = "sync.preferences";
const char kSyncPriorityPreferences[] = "sync.priority_preferences";
const char kSyncSearchEngines[] = "sync.search_engines";
const char kSyncSessions[] = "sync.sessions";
const char kSyncSupervisedUserSettings[] = "sync.managed_user_settings";
const char kSyncSupervisedUserSharedSettings[] =
    "sync.managed_user_shared_settings";
const char kSyncSupervisedUsers[] = "sync.managed_users";
const char kSyncSyncedNotificationAppInfo[] =
    "sync.synced_notification_app_info";
const char kSyncSyncedNotifications[] = "sync.synced_notifications";
const char kSyncTabs[] = "sync.tabs";
const char kSyncThemes[] = "sync.themes";
const char kSyncTypedUrls[] = "sync.typed_urls";

// Boolean used by enterprise configuration management in order to lock down
// sync.
const char kSyncManaged[] = "sync.managed";

// Boolean to prevent sync from automatically starting up.  This is
// used when sync is disabled by the user via the privacy dashboard.
const char kSyncSuppressStart[] = "sync.suppress_start";

// A string that can be used to restore sync encryption infrastructure on
// startup so that the user doesn't need to provide credentials on each start.
const char kSyncEncryptionBootstrapToken[] = "sync.encryption_bootstrap_token";

// Same as kSyncEncryptionBootstrapToken, but derived from the keystore key,
// so we don't have to do a GetKey command at restart.
const char kSyncKeystoreEncryptionBootstrapToken[] =
    "sync.keystore_encryption_bootstrap_token";

// Boolean tracking whether the user chose to specify a secondary encryption
// passphrase.
const char kSyncUsingSecondaryPassphrase[] = "sync.using_secondary_passphrase";

// List of the currently acknowledged set of sync types, used to figure out
// if a new sync type has rolled out so we can notify the user.
const char kSyncAcknowledgedSyncTypes[] = "sync.acknowledged_types";

// The GUID session sync will use to identify this client, even across sync
// disable/enable events.
const char kSyncSessionsGUID[] = "sync.session_sync_guid";

#if defined(OS_CHROMEOS)
// A string that is used to store first-time sync startup after once sync is
// disabled. This will be refreshed every sign-in.
const char kSyncSpareBootstrapToken[] = "sync.spare_bootstrap_token";
#endif  // defined(OS_CHROMEOS)

// Stores how many times to try rollback before giving up.
const char kSyncRemainingRollbackTries[] = "sync.remaining_rollback_tries";

// Stores the timestamp of first sync.
const char kSyncFirstSyncTime[] = "sync.first_sync_time";

}  // namespace prefs

}  // namespace sync_driver
