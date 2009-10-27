// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_STATUS_UI_HELPER_H_
#define CHROME_BROWSER_SYNC_SYNC_STATUS_UI_HELPER_H_

#include "base/string16.h"

class ProfileSyncService;

// Utility to gather current sync status information from the sync service and
// constructs messages suitable for showing in UI.
class SyncStatusUIHelper {
 public:
  enum MessageType {
    PRE_SYNCED,  // User has not set up sync.
    SYNCED,      // We are synced and authenticated to a gmail account.
    SYNC_ERROR,  // A sync error (such as invalid credentials) has occurred.
  };

  // Create status and link labels for the current status labels and link text
  // by querying |service|.
  static MessageType GetLabels(ProfileSyncService* service,
                               string16* status_label,
                               string16* link_label);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncStatusUIHelper);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_STATUS_UI_HELPER_H_
