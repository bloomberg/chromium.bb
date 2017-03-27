// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_

#import <UIKit/UIKit.h>

// ToolbarConsumer sets the current appearance of the Toolbar.
@protocol ToolbarConsumer
// Sets the text for a label appearing in the center of the toolbar.
- (void)setCurrentPageText:(NSString*)text;
// Updates the toolbar with the current forward navigation state.
- (void)setCanGoForward:(BOOL)canGoForward;
// Updates the toolbar with the current back navigation state.
- (void)setCanGoBack:(BOOL)canGoBack;
// Updates the toolbar with the current loading state.
- (void)setIsLoading:(BOOL)isLoading;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSUMER_H_
