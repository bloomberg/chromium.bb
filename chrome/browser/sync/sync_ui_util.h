// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "base/string16.h"
#include "chrome/browser/sync/profile_sync_service.h"

class Profile;

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.
namespace sync_ui_util {

enum MessageType {
  PRE_SYNCED,  // User has not set up sync.
  SYNCED,      // We are synced and authenticated to a gmail account.
  SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
};

// TODO(akalin): audit the use of ProfileSyncService* service below,
// and use const ProfileSyncService& service where possible.

// Create status and link labels for the current status labels and link text
// by querying |service|.
MessageType GetStatusLabels(ProfileSyncService* service,
                            string16* status_label,
                            string16* link_label);

MessageType GetStatus(ProfileSyncService* service);

// Determines whether or not the sync error button should be visible.
bool ShouldShowSyncErrorButton(ProfileSyncService* service);

// Returns a string with the synchronization status.
string16 GetSyncMenuLabel(ProfileSyncService* service);

// Open the appropriate sync dialog for the given profile (which can be
// incognito).  |code| should be one of the START_FROM_* codes.
void OpenSyncMyBookmarksDialog(
    Profile* profile, ProfileSyncService::SyncEventCodes code);
}  // namespace sync_ui_util

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

