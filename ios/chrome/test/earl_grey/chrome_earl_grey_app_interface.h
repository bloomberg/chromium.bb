// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)URL;

// If the current WebState is HTML content, will wait until the window ID is
// injected. Returns YES if the injection is successful or if the WebState is
// not HTML content.
+ (BOOL)waitForWindowIDInjectionIfNeeded;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

// Opens a new tab, and does not wait for animations to complete.
+ (void)openNewTab;

// Closes current tab.
+ (void)closeCurrentTab;

// Opens a new incognito tab, and does not wait for animations to complete.
+ (void)openNewIncognitoTab;

// Closes all tabs in the current mode (incognito or normal), and does not wait
// for the UI to complete. If current mode is Incognito, mode will be switched
// normal after closing all tabs.
+ (void)closeAllTabsInCurrentMode;

// Closes all incognito tabs. Return YES on success.
+ (BOOL)closeAllIncognitoTabs;

// Closes all tabs in all modes (incognito and main (non-incognito)) and does
// not wait for the UI to complete. If current mode is Incognito, mode will be
// switched to main (non-incognito) after closing the incognito tabs.
+ (void)closeAllTabs;

// Navigates back to the previous page without waiting for the page to load.
+ (void)startGoingBack;

// Navigates forward to the next page without waiting for the page to load.
+ (void)startGoingForward;

// Sets value for content setting.
+ (void)setContentSettings:(ContentSetting)setting;

// Signs the user out, clears the known accounts entirely and checks whether
// the accounts were correctly removed from the keychain. Returns nil on
// success, or else an NSError indicating why the operation failed.
+ (NSError*)signOutAndClearAccounts;

// Sets up a fake sync server to be used by the ProfileSyncService.
+ (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the ProfileSyncService and restores
// the real one.
+ (void)tearDownFakeSyncServer;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// Return YES on success.
+ (BOOL)waitForBookmarksToFinishinLoading;

// Clears bookmarks. Returns YES on success.
+ (BOOL)clearBookmarks;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
