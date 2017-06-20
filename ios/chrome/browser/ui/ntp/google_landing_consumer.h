// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONSUMER_H_

#import <Foundation/Foundation.h>

// TODO(crbug.com/694750): Remove these two when the types below are changed.
#include "components/ntp_tiles/ntp_tile.h"
#include "ios/public/provider/chrome/browser/images/whats_new_icon.h"
#include "ios/public/provider/chrome/browser/ui/logo_vendor.h"

// Handles google landing controller update notifications.
@protocol GoogleLandingConsumer<NSObject>

// Whether the Google logo or doodle is being shown.
- (void)setLogoIsShowing:(BOOL)logoIsShowing;

// Exposes view and methods to drive the doodle.
- (void)setLogoVendor:(id<LogoVendor>)logoVendor;

// |YES| if this consumer is has voice search enabled.
- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled;

// Sets the maximum number of sites shown.
- (void)setMaximumMostVisitedSitesShown:
    (NSUInteger)maximumMostVisitedSitesShown;

// Sets the text of a what's new promo.
- (void)setPromoText:(NSString*)promoText;

// Sets the icon of a what's new promo.
// TODO(crbug.com/694750): This should not be WhatsNewIcon.
- (void)setPromoIcon:(WhatsNewIcon)promoIcon;

// |YES| if a what's new promo can be displayed.
- (void)setPromoCanShow:(BOOL)promoCanShow;

// The number of tabs to show in the google landing fake toolbar.
- (void)setTabCount:(int)tabCount;

// |YES| if the google landing toolbar can show the forward arrow.
- (void)setCanGoForward:(BOOL)canGoForward;

// |YES| if the google landing toolbar can show the back arrow.
- (void)setCanGoBack:(BOOL)canGoBack;

// TODO(crbug.com/694750): This should be replaced with consumer suitable data
// type property.
// Tells the consumer to that most visited data updated.
- (void)mostVisitedDataUpdated;

// Tells the consumer a most visited icon was updated.
- (void)mostVisitedIconMadeAvailableAtIndex:(NSUInteger)index;

// TODO(crbug.com/694750): These two calls can be made with dispatcher instead.
// The location bar has lost focus.
- (void)locationBarResignsFirstResponder;

// Tell location bar has taken focus.
- (void)locationBarBecomesFirstResponder;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONSUMER_H_
