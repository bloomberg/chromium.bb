// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_WHATS_NEW_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_WHATS_NEW_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#include "ios/public/provider/chrome/browser/images/whats_new_icon.h"

@protocol WhatsNewHeaderViewDelegate<NSObject>

- (void)onPromoLabelTapped;

@end

@interface WhatsNewHeaderView : UICollectionReusableView

@property(nonatomic, assign) id<WhatsNewHeaderViewDelegate> delegate;

// Sets the text for the attributed label.
- (void)setText:(NSString*)text;

// Sets the icon of the view.
- (void)setIcon:(WhatsNewIcon)icon;

// Sets the value to use for the left and right margin.
- (void)setSideMargin:(CGFloat)sideMargin;

// Returns the minimum height required for WhatsNewHeaderView to fit in |width|,
// for a given |text|.
+ (int)heightToFitText:(NSString*)text inWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_WHATS_NEW_HEADER_VIEW_H_
