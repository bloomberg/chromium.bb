// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"

@interface ContextualSearchHeaderView
    : UIView<ContextualSearchPanelMotionObserver>

@property(nonatomic, assign) id<ContextualSearchPanelTapHandler> tapHandler;

- (instancetype)initWithHeight:(CGFloat)height NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Set the displayed text to |text|, with text in |followingTextRange| colored
// gray.
- (void)setText:(NSString*)text
    followingTextRange:(NSRange)followingTextRange
              animated:(BOOL)animated;

// Set the diplayed text to |searchTerm|.
- (void)setSearchTerm:(NSString*)searchTerm animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_HEADER_VIEW_H_
