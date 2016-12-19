// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_BAR_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_BAR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"

@class NewTabPageBarItem;

@protocol NewTabPageBarDelegate
// Called when new tab page bar item is selected and the selection is changed.
// If |changePanel| is true, bring the panel into view in the scroll view.
- (void)newTabBarItemDidChange:(NewTabPageBarItem*)selectedItem
                   changePanel:(BOOL)changePanel;
@end

// The bar in the new tab page that switches between the provided choices.
// It also draws a notch pointing to the center of the selected choice to
// create a "speech bubble" effect.
@interface NewTabPageBar : UIView<UIGestureRecognizerDelegate>

@property(nonatomic, retain) NSArray* items;
@property(nonatomic, assign) NSUInteger selectedIndex;
@property(nonatomic, assign) CGFloat overlayPercentage;
@property(nonatomic, readonly, retain) NSArray* buttons;
@property(nonatomic, assign) id<NewTabPageBarDelegate> delegate;

// Changes the colors of the buttons and overlay depending on the content offset
// of the scroll view. Tablet Incognito only.
- (void)updateColorsForScrollView:(UIScrollView*)scrollView;

// Updates the alpha of the shadow image. When the alpha changes from 0 to 1 or
// 1 to 0, the alpha change is animated.
- (void)setShadowAlpha:(CGFloat)alpha;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_BAR_H_
