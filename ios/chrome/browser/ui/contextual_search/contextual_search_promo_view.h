// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_protocols.h"

@protocol ContextualSearchPromoViewDelegate<NSObject>

- (void)promoViewAcceptTapped;
- (void)promoViewDeclineTapped;
- (void)promoViewSettingsTapped;

@end

@interface ContextualSearchPromoView
    : UIView<ContextualSearchPanelMotionObserver>

// If YES, this view will remain hidden (setHidden:YES will no-op).
@property(nonatomic, assign) BOOL disabled;

// Designated initializer. The caller is responsible for setting constraints
// such that this view is the desired size.
- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<ContextualSearchPromoViewDelegate>)delegate;

- (void)closeAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PROMO_VIEW_H_
