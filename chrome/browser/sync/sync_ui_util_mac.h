// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_UI_UTIL_MAC_H_
#define CHROME_BROWSER_SYNC_SYNC_UI_UTIL_MAC_H_
#pragma once

#include "chrome/browser/sync/sync_ui_util.h"

#import <Cocoa/Cocoa.h>

class Profile;

namespace sync_ui_util {

// Updates a bookmark sync UI item (expected to be a menu item). This is
// called every time a menu containing a sync UI item is displayed.
void UpdateSyncItem(id syncItem, BOOL syncEnabled, Profile* profile);

// This function (used by UpdateSyncItem) is only exposed for testing.
// Just use UpdateSyncItem() instead.
void UpdateSyncItemForStatus(id syncItem, BOOL syncEnabled,
                             sync_ui_util::MessageType status);

}  // namespace sync_ui_util

#endif  // CHROME_BROWSER_SYNC_SYNC_UI_UTIL_MAC_H_

