// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_

#include "base/strings/string16.h"

class ProfileSyncService;
class SigninManagerBase;

// Utility functions to gather current sync status information from the sync
// service and constructs messages suitable for showing in UI.
namespace sync_ui_util {

enum MessageType {
  PRE_SYNCED,  // User has not set up sync.
  SYNCED,      // We are synced and authenticated to a gmail account.
  SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
  SYNC_PROMO,  // A situation has occurred which should be brought to the user's
               // attention, but not as an error.
};

enum StatusLabelStyle {
  PLAIN_TEXT,  // Label will be plain-text only.
  WITH_HTML    // Label may contain an HTML-formatted link.
};

// TODO(akalin): audit the use of ProfileSyncService* service below,
// and use const ProfileSyncService& service where possible.

// Create status and link labels for the current status labels and link text
// by querying |service|.
// |style| sets the link properties, see |StatusLabelStyle|.
MessageType GetStatusLabels(ProfileSyncService* service,
                            const SigninManagerBase& signin,
                            StatusLabelStyle style,
                            base::string16* status_label,
                            base::string16* link_label);

// Same as above but for use specifically on the New Tab Page.
// |status_label| may contain an HTML-formatted link.
MessageType GetStatusLabelsForNewTabPage(ProfileSyncService* service,
                                         const SigninManagerBase& signin,
                                         base::string16* status_label,
                                         base::string16* link_label);

// Gets various labels for the sync global error based on the sync error state.
// |menu_item_label|, |bubble_message|, and |bubble_accept_label| must not be
// NULL. Note that we don't use SyncGlobalError on Chrome OS.
#if !defined(OS_CHROMEOS)
void GetStatusLabelsForSyncGlobalError(const ProfileSyncService* service,
                                       base::string16* menu_item_label,
                                       base::string16* bubble_message,
                                       base::string16* bubble_accept_label);
#endif

MessageType GetStatus(ProfileSyncService* service,
                      const SigninManagerBase& signin);

}  // namespace sync_ui_util
#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_H_
