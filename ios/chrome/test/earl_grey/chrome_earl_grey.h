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

#pragma mark - Navigation Utilities

// Loads |URL| in the current WebState with transition of type
// ui::PAGE_TRANSITION_TYPED, and waits for the page to complete loading, or
// a timeout.
+ (void)loadURL:(GURL)URL;

// Waits for the page to finish loading or a timeout.
+ (void)waitForPageToFinishLoading;

// Taps html element with |elementID| in the current web view.
+ (void)tapWebViewElementWithID:(NSString*)elementID;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_H_
