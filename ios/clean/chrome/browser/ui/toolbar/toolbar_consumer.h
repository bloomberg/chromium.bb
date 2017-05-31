// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_

#import <UIKit/UIKit.h>

// ToolbarConsumer sets the current appearance of the Toolbar.
@protocol ToolbarConsumer
// Updates the toolbar with the current forward navigation state.
- (void)setCanGoForward:(BOOL)canGoForward;
// Updates the toolbar with the current back navigation state.
- (void)setCanGoBack:(BOOL)canGoBack;
// Updates the toolbar with the current loading state.
- (void)setIsLoading:(BOOL)isLoading;
// Updates the toolbar with the current progress of the loading WebState.
- (void)setLoadingProgress:(double)progress;
// Sets whether the toolbar should display for a visible tab strip or not.
- (void)setTabStripVisible:(BOOL)visible;
// Updates the toolbar with the current number of total tabs.
- (void)setTabCount:(int)tabCount;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
