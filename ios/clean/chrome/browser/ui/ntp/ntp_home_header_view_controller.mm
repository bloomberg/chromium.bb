// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeHeaderViewController ()

// Redifined as readwrite
@property(nonatomic, assign, getter=isOmniboxFocused, readwrite)
    BOOL omniboxFocused;

#pragma mark properties from GoogleLandingConsumer

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;
// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

@end
@implementation NTPHomeHeaderViewController

@synthesize omniboxFocused = _omniboxFocused;
@synthesize logoIsShowing = _logoIsShowing;
@synthesize voiceSearchIsEnabled = _voiceSearchIsEnabled;
@synthesize logoVendor = _logoVendor;

@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize collectionSynchronizer = _collectionSynchronizer;

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  return nil;
}

#pragma mark - ContentSuggestionsHeaderControlling

- (void)updateSearchFieldForOffset:(CGFloat)offset {
  // TODO: implement this.
}

- (void)unfocusOmnibox {
  // TODO: implement this.
}

- (void)layoutHeader {
  // TODO: implement this.
}

#pragma mark - GoogleLandingConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  // TODO: implement this.
}

- (void)setMaximumMostVisitedSitesShown:
    (NSUInteger)maximumMostVisitedSitesShown {
  // Do nothing as it is handled elsewhere.
}

- (void)setPromoText:(NSString*)promoText {
  // Do nothing as it is handled elsewhere.
}

- (void)setPromoIcon:(WhatsNewIcon)promoIcon {
  // Do nothing as it is handled elsewhere.
}

- (void)setPromoCanShow:(BOOL)promoCanShow {
  // Do nothing as it is handled elsewhere.
}

- (void)setTabCount:(int)tabCount {
  // Do nothing as it is handled elsewhere.
}

- (void)setCanGoForward:(BOOL)canGoForward {
  // Do nothing as it is handled elsewhere.
}

- (void)setCanGoBack:(BOOL)canGoBack {
  // Do nothing as it is handled elsewhere.
}

- (void)mostVisitedDataUpdated {
  // Do nothing as it is handled elsewhere.
}

- (void)mostVisitedIconMadeAvailableAtIndex:(NSUInteger)index {
  // Do nothing as it is handled elsewhere.
}

- (void)locationBarResignsFirstResponder {
  // TODO: implement this.
}

- (void)locationBarBecomesFirstResponder {
  // TODO: implement this.
}

@end
