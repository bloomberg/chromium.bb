// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_

#import <EarlGrey/EarlGrey.h>

#include "url/gurl.h"

namespace chrome_test_util {

// TODO(crbug.com/638674): Evaluate if this can move to shared code.
// Execute |javascript| on current web state, and wait for either the completion
// of execution or timeout. If |out_error| is not nil, it is set to the
// error resulting from the execution, if one occurs. The return value is the
// result of the JavaScript execution. If the request is timed out, then nil is
// returned.
// TODO(crbug.com/690057): Remove __unsafe_unretained once all callers are
// converted to ARC.
id ExecuteJavaScript(NSString* javascript,
                     NSError* __unsafe_unretained* out_error);

}  // namespace chrome_test_util

// Test methods that perform actions on Chrome. These methods may read or alter
// Chrome's internal state programmatically or via the UI, but in both cases
// will properly synchronize the UI for Earl Grey tests.
@interface ChromeEarlGrey : NSObject

#pragma mark - History Utilities

// Clears browsing history.
+ (void)clearBrowsingHistory;

#pragma mark - Cookie Utilities

// Returns cookies as key value pairs, where key is a cookie name and value is a
// cookie value.
// NOTE: this method fails the test if there are errors getting cookies.
+ (NSDictionary*)cookies;

#pragma mark - Navigation Utilities

// Loads |URL| in the current WebState with transition type
// ui::PAGE_TRANSITION_TYPED, and waits for the loading to complete within a
// timeout, or a GREYAssert is induced.
+ (void)loadURL:(GURL)URL;

// Reloads the page and waits for the loading to complete within a timeout, or a
// GREYAssert is induced.
+ (void)reload;

// Navigates back to the previous page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
+ (void)goBack;

// Navigates forward to the next page and waits for the loading to complete
// within a timeout, or a GREYAssert is induced.
+ (void)goForward;

// Waits for the page to finish loading within a timeout, or a GREYAssert is
// induced.
+ (void)waitForPageToFinishLoading;

// Taps html element with |elementID| in the current web view.
+ (void)tapWebViewElementWithID:(NSString*)elementID;

// Waits for a static html view containing |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
+ (void)waitForStaticHTMLViewContainingText:(NSString*)text;

// Waits for there to be no static html view, or a static html view that does
// not contain |text|. If the condition is not met within a timeout, a
// GREYAssert is induced.
+ (void)waitForStaticHTMLViewNotContainingText:(NSString*)text;

// Waits for a Chrome error page. If it is not found within a timeout, a
// GREYAssert is induced.
+ (void)waitForErrorPage;
@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
