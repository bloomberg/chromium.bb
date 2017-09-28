// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_H_

#import <UIKit/UIKit.h>

@class NewTabPageBar;

// Container view for the new tab page so that the subviews don't get directly
// accessed from the view controller.
@interface NewTabPageView : UIView
@property(nonatomic, weak, readonly) UIScrollView* scrollView;
@property(nonatomic, weak, readonly) NewTabPageBar* tabBar;

- (instancetype)initWithFrame:(CGRect)frame
                andScrollView:(UIScrollView*)scrollView
                    andTabBar:(NewTabPageBar*)tabBar NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// initWithCoder would only be needed for building this view through .xib files.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Updates scrollView's content size to accommodate its panels.
- (void)updateScrollViewContentSize;

// Returns the frame of the item's view within the scrollView's content.
// |index| must be a valid index for tabBar's |items|.
- (CGRect)panelFrameForItemAtIndex:(NSUInteger)index;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_VIEW_H_
