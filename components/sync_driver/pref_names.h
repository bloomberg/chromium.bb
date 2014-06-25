// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various sync preferences, for easier changing.

#include "build/build_config.h"

namespace sync_driver {

namespace prefs {

extern const char kEnhancedBookmarksExperimentEnabled[];
extern const char kEnhancedBookmarksExtensionId[];

extern const char kSyncLastSyncedTime[];
extern const char kSyncHasAuthError[];
extern const char kSyncHasSetupCompleted[];
extern const char kSyncKeepEverythingSynced[];

extern const char kSyncAppList[];
extern const char kSyncAppNotifications[];
extern const char kSyncAppSettings[];
extern const char kSyncApps[];
extern const char kSyncArticles[];
extern const char kSyncAutofillProfile[];
extern const char kSyncAutofill[];
extern const char kSyncBookmarks[];
extern const char kSyncDictionary[];
extern const char kSyncExtensionSettings[];
extern const char kSyncExtensions[];
extern const char kSyncFaviconImages[];
extern const char kSyncFaviconTracking[];
extern const char kSyncHistoryDeleteDirectives[];
extern const char kSyncPasswords[];
extern const char kSyncPreferences[];
extern const char kSyncPriorityPreferences[];
extern const char kSyncSearchEngines[];
extern const char kSyncSessions[];
extern const char kSyncSupervisedUserSettings[];
extern const char kSyncSupervisedUserSharedSettings[];
extern const char kSyncSupervisedUsers[];
extern const char kSyncSyncedNotificationAppInfo[];
extern const char kSyncSyncedNotifications[];
extern const char kSyncTabs[];
extern const char kSyncThemes[];
extern const char kSyncTypedUrls[];

extern const char kSyncManaged[];
extern const char kSyncSuppressStart[];

extern const char kSyncUsingSecondaryPassphrase[];
extern const char kSyncEncryptionBootstrapToken[];
extern const char kSyncKeystoreEncryptionBootstrapToken[];

extern const char kSyncAcknowledgedSyncTypes[];

extern const char kSyncSessionsGUID[];

#if defined(OS_CHROMEOS)
extern const char kSyncSpareBootstrapToken[];
#endif  // defined(OS_CHROMEOS)

extern const char kSyncRemainingRollbackTries[];
extern const char kSyncFirstSyncTime[];

}  // namespace prefs

}  // namespace sync_driver
