// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_

#import <Foundation/Foundation.h>

#include <string>

#include "base/compiler_specific.h"
#include "url/gurl.h"

@class ElementSelector;
@protocol GREYMatcher;

namespace chrome_test_util {

// TODO(crbug.com/788813): Evaluate if JS helpers can be consolidated.
// Execute |javascript| on current web state, and wait for either the completion
// of execution or timeout. If |out_error| is not nil, it is set to the
// error resulting from the execution, if one occurs. The return value is the
// result of the JavaScript execution. If the request is timed out, then nil is
// returned.
id ExecuteJavaScript(NSString* javascript, NSError* __autoreleasing* out_error);

}  // namespace chrome_test_util

// Test methods that perform actions on Chrome. These methods may read or alter
// Chrome's internal state programmatically or via the UI, but in both cases
// will properly synchronize the UI for Earl Grey tests.
@interface ChromeEarlGrey : NSObject

#pragma mark - History Utilities
// Clears browsing history.
+ (NSError*)clearBrowsingHistory WARN_UNUSED_RESULT;

@end

// Helpers that only compile under EarlGrey 1 are included in this "EG1"
// category.
// TODO(crbug.com/922813): Update these helpers to compile under EG2 and move
// them into the main class declaration as they are converted.
@interface ChromeEarlGrey (EG1)

#pragma mark - Cookie Utilities

// Returns cookies as key value pairs, where key is a cookie name and value is a
// cookie value.
// NOTE: this method fails the test if there are errors getting cookies.
+ (NSDictionary*)cookies;

#pragma mark - Navigation Utilities

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and waits for the loading to complete within a
// timeout, or a GREYAssert is induced.
+ (NSError*)loadURL:(const GURL&)URL WARN_UNUSED_RESULT;

// Reloads the page and waits for the loading to complete within a timeout, or a
// GREYAssert is induced.
+ (NSError*)reload WARN_UNUSED_RESULT;

// Navigates back to the previous page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
+ (NSError*)goBack WARN_UNUSED_RESULT;

// Navigates forward to the next page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
+ (NSError*)goForward WARN_UNUSED_RESULT;

// Opens a new tab and waits for the new tab animation to complete.
+ (NSError*)openNewTab WARN_UNUSED_RESULT;

// Opens a new incognito tab and waits for the new tab animation to complete.
+ (NSError*)openNewIncognitoTab WARN_UNUSED_RESULT;

// Closes all tabs in the current mode (incognito or normal), and waits for the
// UI to complete. If current mode is Incognito, mode will be switched to
// normal after closing all tabs.
+ (void)closeAllTabsInCurrentMode;

// Closes all incognito tabs and waits for the UI to complete.
+ (NSError*)closeAllIncognitoTabs WARN_UNUSED_RESULT;

// Closes the current tab and waits for the UI to complete.
+ (void)closeCurrentTab;

// Waits for the page to finish loading within a timeout, or a GREYAssert is
// induced.
+ (NSError*)waitForPageToFinishLoading WARN_UNUSED_RESULT;

// Taps html element with |elementID| in the current web view.
+ (NSError*)tapWebViewElementWithID:(NSString*)elementID WARN_UNUSED_RESULT;

// Waits for a static html view containing |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
+ (NSError*)waitForStaticHTMLViewContainingText:(NSString*)text
    WARN_UNUSED_RESULT;

// Waits for there to be no static html view, or a static html view that does
// not contain |text|. If the condition is not met within a timeout, a
// GREYAssert is induced.
+ (NSError*)waitForStaticHTMLViewNotContainingText:(NSString*)text
    WARN_UNUSED_RESULT;

// Waits for a Chrome error page. If it is not found within a timeout, a
// GREYAssert is induced.
+ (NSError*)waitForErrorPage WARN_UNUSED_RESULT;

// Waits for the current web view to contain |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
+ (NSError*)waitForWebViewContainingText:(std::string)text WARN_UNUSED_RESULT;

// Waits for the current web view to contain an element matching |selector|.
// If the condition is not met within a timeout, a GREYAssert is induced.
+ (NSError*)waitForWebViewContainingElement:(ElementSelector*)selector
    WARN_UNUSED_RESULT;

// Waits for there to be no web view containing |text|. If the condition is not
// met within a timeout, a GREYAssert is induced.
+ (NSError*)waitForWebViewNotContainingText:(std::string)text
    WARN_UNUSED_RESULT;

// Waits for there to be |count| number of non-incognito tabs. If the condition
// is not met within a timeout, a GREYAssert is induced.
+ (NSError*)waitForMainTabCount:(NSUInteger)count WARN_UNUSED_RESULT;

// Waits for there to be |count| number of incognito tabs. If the condition is
// not met within a timeout, a GREYAssert is induced.
+ (NSError*)waitForIncognitoTabCount:(NSUInteger)count WARN_UNUSED_RESULT;

// Waits for there to be a web view containing a blocked |image_id|.  When
// blocked, the image element will be smaller than the actual image size.
+ (NSError*)waitForWebViewContainingBlockedImageElementWithID:
    (std::string)imageID WARN_UNUSED_RESULT;

// Waits for there to be a web view containing loaded image with |image_id|.
// When loaded, the image element will have the same size as actual image.
+ (NSError*)waitForWebViewContainingLoadedImageElementWithID:
    (std::string)imageID WARN_UNUSED_RESULT;

// Waits for the bookmark internal state to be done loading. If it does not
// happen within a timeout, a GREYAssert is induced.
+ (NSError*)waitForBookmarksToFinishLoading WARN_UNUSED_RESULT;

// Clears bookmarks and if any bookmark still presents. Returns nil on success,
// or else an NSError indicating why the operation failed.
+ (NSError*)clearBookmarks;

// Waits for the matcher to return an element that is sufficiently visible.
+ (NSError*)waitForElementWithMatcherSufficientlyVisible:
    (id<GREYMatcher>)matcher WARN_UNUSED_RESULT;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
