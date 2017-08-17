// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_mediator.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPHomeHeaderMediator ()

// Redifined as readwrite
@property(nonatomic, assign, getter=isOmniboxFocused, readwrite)
    BOOL omniboxFocused;

@property(nonatomic, assign) BOOL promoCanShow;

@end

@implementation NTPHomeHeaderMediator

@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize alerter = _alerter;
@synthesize headerProvider = _headerProvider;
@synthesize headerConsumer = _headerConsumer;

@synthesize isShowing = _isShowing;
@synthesize omniboxFocused = _omniboxFocused;
@synthesize promoCanShow = _promoCanShow;

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  return [self.headerProvider headerForWidth:width];
}

#pragma mark - ContentSuggestionsHeaderControlling

- (void)updateFakeOmniboxForOffset:(CGFloat)offset width:(CGFloat)width {
  [self.headerConsumer updateFakeOmniboxForOffset:offset];
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  [self.headerConsumer updateFakeOmniboxForWidth:width];
}

- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    // TODO(crbug.com/740793): Remove alert once the protocol to send commands
    // to the toolbar is implemented.
    [self.alerter showAlert:@"Cancel omnibox edit"];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)layoutHeader {
  [self.headerConsumer layoutHeader];
}

#pragma mark - GoogleLandingConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  if (self.headerProvider.logoVendor.showingLogo != logoIsShowing) {
    [self.headerConsumer setLogoIsShowing:logoIsShowing];
    [self.collectionSynchronizer invalidateLayout];
  }
}

- (void)setLogoVendor:(id<LogoVendor>)logoVendor {
  self.headerProvider.logoVendor = logoVendor;
}

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled {
  [self.headerConsumer setVoiceSearchIsEnabled:voiceSearchIsEnabled];
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
  if (!self.isShowing && ![self.delegate isScrolledToTop])
    return;

  self.omniboxFocused = NO;
  if ([self.delegate isContextMenuVisible]) {
    return;
  }

  [self shiftCollectionDown];
}

- (void)locationBarBecomesFirstResponder {
  if (!self.isShowing)
    return;

  self.omniboxFocused = YES;
  [self shiftCollectionUp];
}

#pragma mark - ContentSuggestionsViewControllerDelegate

- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.headerProvider.logoVendor.showingLogo, self.promoCanShow, NO);
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(
      self.headerProvider.logoVendor.showingLogo, self.promoCanShow, NO);
}

#pragma mark - Private

- (void)shiftCollectionDown {
  [self.headerConsumer collectionWillShiftDown];
  [self.collectionSynchronizer shiftTilesDown];
  [self.commandHandler dismissModals];
}

- (void)shiftCollectionUp {
  void (^completionBlock)() = ^{
    [self.headerConsumer collectionDidShiftUp];
  };
  [self.collectionSynchronizer shiftTilesUpWithCompletionBlock:completionBlock];
}

@end
