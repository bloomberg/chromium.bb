// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SYNC_SYNC_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SYNC_SYNC_UTIL_H_

#import <Foundation/Foundation.h>

#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"

@class GenericChromeCommand;
@class Tab;

namespace ios {
class ChromeBrowserState;
}

namespace ios_internal {
namespace sync {

// Gets the top-level description message associated with the sync error state
// of |browserState|. Returns nil if there is no sync error.
NSString* GetSyncErrorDescriptionForBrowserState(
    ios::ChromeBrowserState* browserState);

// Gets the string message associated with the sync error state of
// |browserState|. The returned error message does not contain any links.
// Returns nil if there is no sync error.
NSString* GetSyncErrorMessageForBrowserState(
    ios::ChromeBrowserState* browserState);

// Gets the title of the button to fix the sync error of |browserState|.
// Returns nil if there is no sync error or it can't be fixed by a user action.
NSString* GetSyncErrorButtonTitleForBrowserState(
    ios::ChromeBrowserState* browserState);

// Gets the command associated with the sync state of |browserState|.
GenericChromeCommand* GetSyncCommandForBrowserState(
    ios::ChromeBrowserState* browserState);

// Check for sync errors, and display any that ought to be shown to the user.
// Returns true if an infobar was brought up.
bool displaySyncErrors(ios::ChromeBrowserState* browser_state, Tab* tab);

// Returns true if |errorState| corresponds to a transient sync error.
bool IsTransientSyncError(SyncSetupService::SyncServiceState errorState);

}  // namespace sync
}  // namespace ios_internal

#endif  // IOS_CHROME_BROWSER_UI_SYNC_SYNC_UTIL_H_
