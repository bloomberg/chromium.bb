// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

// Test methods that perform actions on Chrome. These methods only affect Chrome
// using the UI with Earl Grey.
@interface ChromeEarlGreyUI : NSObject

// Makes the toolbar visible by swiping downward, if necessary. Then taps on
// the Tools menu button. At least one tab needs to be open and visible when
// calling this method.
+ (void)openToolsMenu;

// Open a new tab via the tools menu.
+ (void)openNewTab;

// Open a new incognito tab via the tools menu.
+ (void)openNewIncognitoTab;

// Reloads the page via the reload button, and does not wait for the page to
// finish loading.
+ (void)reload;

// Opens the share menu via the share button.
// This method requires that there is at least one tab open.
+ (void)openShareMenu;

// Waits for toolbar to become visible if |isVisible| is YES, otherwise waits
// for it to disappear. If the condition is not met within a timeout, a
// GREYAssert is induced.
+ (void)waitForToolbarVisible:(BOOL)isVisible;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_EARL_GREY_UI_H_
