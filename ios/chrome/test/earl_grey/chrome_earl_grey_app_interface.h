// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"
#import "components/sync/base/model_type.h"

@class ElementSelector;

// ChromeEarlGreyAppInterface contains the app-side implementation for helpers
// that primarily work via direct model access. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface ChromeEarlGreyAppInterface : NSObject

// Clears browsing history and waits for history to finish clearing before
// returning. Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)clearBrowsingHistory;

// Loads the URL |spec| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED and returns without waiting for the page to load.
+ (void)startLoadingURL:(NSString*)spec;

// If the current WebState is HTML content, will wait until the window ID is
// injected. Returns YES if the injection is successful or if the WebState is
// not HTML content.
+ (BOOL)waitForWindowIDInjectionIfNeeded;

// Returns YES if the current WebState is loading.
+ (BOOL)isLoading;

// Reloads the page without waiting for the page to load.
+ (void)startReloading;

#pragma mark - Tab Utilities (EG2)

// Selects tab with given index in current mode (incognito or main
// (non-incognito)).
+ (void)selectTabAtIndex:(NSUInteger)index;

// Closes tab with the given index in current mode (incognito or main
// (non-incognito)).
+ (void)closeTabAtIndex:(NSUInteger)index;

// Returns YES if the browser is in incognito mode, and NO otherwise.
+ (BOOL)isIncognitoMode WARN_UNUSED_RESULT;

// Returns the number of open non-incognito tabs.
+ (NSUInteger)mainTabCount WARN_UNUSED_RESULT;

// Returns the number of open incognito tabs.
+ (NSUInteger)incognitoTabCount WARN_UNUSED_RESULT;

// Simulates a backgrounding.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)simulateTabsBackgrounding;

// Returns the number of main (non-incognito) tabs currently evicted.
+ (NSUInteger)evictedMainTabCount WARN_UNUSED_RESULT;

// Evicts the tabs associated with the non-current browser mode.
+ (void)evictOtherTabModelTabs;

// Sets the normal tabs as 'cold start' tabs
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)setCurrentTabsToBeColdStartTabs;

// Resets the tab usage recorder on current mode.
// If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)resetTabUsageRecorder;

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

// Closes all incognito tabs. If not succeed returns an NSError indicating  why
// the operation failed, otherwise nil.
+ (NSError*)closeAllIncognitoTabs;

// Closes all tabs in all modes (incognito and main (non-incognito)) and does
// not wait for the UI to complete. If current mode is Incognito, mode will be
// switched to main (non-incognito) after closing the incognito tabs.
+ (void)closeAllTabs;

// Navigates back to the previous page without waiting for the page to load.
+ (void)startGoingBack;

// Navigates forward to the next page without waiting for the page to load.
+ (void)startGoingForward;

// Returns the title of the current selected tab.
+ (NSString*)currentTabTitle;

// Returns the title of the next tab. Assumes that there is a next tab.
+ (NSString*)nextTabTitle;

// Returns a unique identifier for the current Tab.
+ (NSString*)currentTabID;

// Returns a unique identifier for the next Tab.
+ (NSString*)nextTabID;

#pragma mark - WebState Utilities (EG2)

// Attempts to tap the element with |element_id| within window.frames[0] of the
// current WebState using a JavaScript click() event. This only works on
// same-origin iframes. If not succeed returns an NSError indicating why the
// operation failed, otherwise nil.
+ (NSError*)tapWebStateElementInIFrameWithID:(NSString*)elementID;

// Taps html element with |elementID| in the current web state.
// On failure returns NO and |error| is set to include a message.
+ (BOOL)tapWebStateElementWithID:(NSString*)elementID error:(NSError*)error;

// Waits for the current web state to contain an element matching |selector|.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForWebStateContainingElement:(ElementSelector*)selector;

// Attempts to submit form with |formID| in the current WebState.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
+ (NSError*)submitWebStateFormWithID:(NSString*)formID;

// Returns YES if the current WebState contains |text|.
+ (BOOL)webStateContainsText:(NSString*)text;

// Waits for the current WebState to contain loaded image with |imageID|.
// When loaded, the image element will have the same size as actual image.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingLoadedImage:(NSString*)imageID;

// Waits for the current WebState to contain a blocked image with |imageID|.
// When blocked, the image element will be smaller than the actual image size.
// Returns nil if the condition is met within a timeout, or else an NSError
// indicating why the operation failed.
+ (NSError*)waitForWebStateContainingBlockedImage:(NSString*)imageID;

// Sets value for content setting.
+ (void)setContentSettings:(ContentSetting)setting;

// Signs the user out, clears the known accounts entirely and checks whether
// the accounts were correctly removed from the keychain. Returns nil on
// success, or else an NSError indicating why the operation failed.
+ (NSError*)signOutAndClearAccounts;

// Returns the current WebState's VisibleURL.
+ (NSString*)webStateVisibleURL;

// Purges cached web view pages in the current web state, so the next time back
// navigation will not use a cached page. Browsers don't have to use a fresh
// version for back/forward navigation for HTTP pages and may serve a version
// from the cache even if the Cache-Control response header says otherwise.
+ (void)purgeCachedWebViewPages;

// Returns YES if the current WebState's navigation manager is currently
// restoring session state.
+ (BOOL)isRestoreSessionInProgress;

#pragma mark - Sync Utilities (EG2)

// Clears the autofill profile for the given |GUID|.
+ (void)clearAutofillProfileWithGUID:(NSString*)GUID;

// Injects an autofill profile into the fake sync server with |GUID| and
// |full_name|.
+ (void)injectAutofillProfileOnFakeSyncServerWithGUID:(NSString*)GUID
                                  autofillProfileName:(NSString*)fullName;

// Returns YES if there is an autofilll profile with the corresponding |GUID|
// and |full_name|.
+ (BOOL)isAutofillProfilePresentWithGUID:(NSString*)GUID
                     autofillProfileName:(NSString*)fullName;

#pragma mark - Bookmarks Utilities (EG2)

// Waits for the bookmark internal state to be done loading.
// If not succeed returns an NSError indicating  why the operation failed,
// otherwise nil.
+ (NSError*)waitForBookmarksToFinishinLoading;

// Clears bookmarks. If not succeed returns an NSError indicating  why the
// operation failed, otherwise nil.
+ (NSError*)clearBookmarks;

#pragma mark - URL Utilities (EG2)

// Returns the title string to be used for a page with |URL| if that page
// doesn't specify a title.
+ (NSString*)displayTitleForURL:(NSString*)URL;

#pragma mark - Autofill Utilities (EG2)

// Removes the stored credit cards.
+ (void)clearCreditCards;

#pragma mark - Sync Utilities (EG2)

// Clears fake sync server data.
+ (void)clearSyncServerData;

// Starts the sync server. The server should not be running when calling this.
+ (void)startSync;

// Stops the sync server. The server should be running when calling this.
+ (void)stopSync;

// Waits for sync to be initialized or not.
// Returns nil on success, or else an NSError indicating why the
// operation failed.
+ (NSError*)waitForSyncInitialized:(BOOL)isInitialized
                       syncTimeout:(NSTimeInterval)timeout;

// Returns the current sync cache GUID. The sync server must be running when
// calling this.
+ (NSString*)syncCacheGUID;

// Sets up a fake sync server to be used by the ProfileSyncService.
+ (void)setUpFakeSyncServer;

// Tears down the fake sync server used by the ProfileSyncService and restores
// the real one.
+ (void)tearDownFakeSyncServer;

// Gets the number of entities of the given |type|.
+ (int)numberOfSyncEntitiesWithType:(syncer::ModelType)type;

// Injects a bookmark into the fake sync server with |URL| and |title|.
+ (void)addFakeSyncServerBookmarkWithURL:(NSString*)URL title:(NSString*)title;

// Injects typed URL to sync FakeServer.
+ (void)addFakeSyncServerTypedURL:(NSString*)URL;

// Adds typed URL into HistoryService.
+ (void)addHistoryServiceTypedURL:(NSString*)URL;

// Deletes typed URL from HistoryService.
+ (void)deleteHistoryServiceTypedURL:(NSString*)URL;

// If the provided URL |spec| is either present or not present in HistoryService
// (depending on |expectPresent|), return YES. If the present status of |spec|
// is not what is expected, or there is an error, return NO.
+ (BOOL)isTypedURL:(NSString*)spec presentOnClient:(BOOL)expectPresent;

// Triggers a sync cycle for a |type|.
+ (void)triggerSyncCycleForType:(syncer::ModelType)type;

// Deletes an autofill profile from the fake sync server with |GUID|, if it
// exists. If it doesn't exist, nothing is done.
+ (void)deleteAutofillProfileOnFakeSyncServerWithGUID:(NSString*)GUID;

// Verifies the sessions hierarchy on the Sync FakeServer. |specs| is
// the collection of URLs that are to be expected for a single window. On
// failure, returns a NSError describing the failure. See the
// SessionsHierarchy class for documentation regarding the verification.
+ (NSError*)verifySessionsOnSyncServerWithSpecs:(NSArray<NSString*>*)specs;

// Verifies that |count| entities of the given |type| and |name| exist on the
// sync FakeServer. Folders are not included in this count. Returns NSError
// if there is a failure or if the count does not match.
+ (NSError*)verifyNumberOfSyncEntitiesWithType:(NSUInteger)type
                                          name:(NSString*)name
                                         count:(NSUInteger)count;

#pragma mark - JavaScript Utilities (EG2)

// Executes JavaScript on current WebState, and waits for either the completion
// or timeout. If execution does not complete within a timeout or JavaScript
// exception is thrown, returns an NSError indicating why the operation failed,
// otherwise returns object representing execution result.
+ (id)executeJavaScript:(NSString*)javaScript error:(NSError**)error;

#pragma mark - Accessibility Utilities (EG2)

// Verifies that all interactive elements on screen (or at least one of their
// descendants) are accessible.
+ (NSError*)verifyAccessibilityForCurrentScreen WARN_UNUSED_RESULT;

#pragma mark - Check features (EG2)

// Helpers for checking feature state. These can't use FeatureList directly when
// invoked from test code, as the EG test code runs in a separate process and
// must query Chrome for the state.

// Returns YES if SlimNavigationManager feature is enabled.
+ (BOOL)isSlimNavigationManagerEnabled WARN_UNUSED_RESULT;

// Returns YES if BlockNewTabPagePendingLoad feature is enabled.
+ (BOOL)isBlockNewTabPagePendingLoadEnabled WARN_UNUSED_RESULT;

// Returns YES if NewOmniboxPopupLayout feature is enabled.
+ (BOOL)isNewOmniboxPopupLayoutEnabled WARN_UNUSED_RESULT;

// Returns YES if UmaCellular feature is enabled.
+ (BOOL)isUMACellularEnabled WARN_UNUSED_RESULT;

// Returns YES if UKM feature is enabled.
+ (BOOL)isUKMEnabled WARN_UNUSED_RESULT;

// Returns YES if WebPaymentsModifiers feature is enabled.
+ (BOOL)isWebPaymentsModifiersEnabled WARN_UNUSED_RESULT;

// Returns YES if SettingsAddPaymentMethod feature is enabled.
+ (BOOL)isSettingsAddPaymentMethodEnabled WARN_UNUSED_RESULT;

// Returns YES if CreditCardScanner feature is enabled.
+ (BOOL)isCreditCardScannerEnabled WARN_UNUSED_RESULT;

// Returns YES if custom WebKit frameworks were properly loaded, rather than
// system frameworks. Always returns YES if the app was not requested to run
// with custom WebKit frameworks.
+ (BOOL)isCustomWebKitLoadedIfRequested WARN_UNUSED_RESULT;

#pragma mark - Popup Blocking

// Gets the current value of the popup content setting preference for the
// original browser state.
+ (ContentSetting)popupPrefValue;

// Sets the popup content setting preference to the given value for the original
// browser state.
+ (void)setPopupPrefValue:(ContentSetting)value;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_APP_INTERFACE_H_
