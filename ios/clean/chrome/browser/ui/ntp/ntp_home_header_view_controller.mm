// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_home_header_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const UIEdgeInsets kSearchBoxStretchInsets = {3, 3, 3, 3};
}  // namespace

@interface NTPHomeHeaderViewController ()

// |YES| if the header view is visible.  When set to |NO| various UI updates are
// ignored, like shifting the tiles up when the omnibox is focused.
@property(nonatomic, assign) BOOL isShowing;

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

@property(nonatomic, assign) BOOL promoCanShow;
// Exposes view and methods to drive the doodle.
@property(nonatomic, weak, nullable) id<LogoVendor> logoVendor;

// Whether the omnibox is currently focused.
@property(nonatomic, assign, getter=isOmniboxFocused) BOOL omniboxFocused;

@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* voiceTapTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, strong) UIImageView* fakeOmniboxBorder;
@property(nonatomic, strong) UIImageView* fakeOmniboxShadow;

@end

@implementation NTPHomeHeaderViewController

@synthesize voiceSearchIsEnabled = _voiceSearchIsEnabled;
@synthesize logoVendor = _logoVendor;

@synthesize omniboxFocused = _omniboxFocused;
@synthesize fakeOmnibox = _fakeOmnibox;
@synthesize hintLabelLeadingConstraint = _hintLabelLeadingConstraint;
@synthesize voiceTapTrailingConstraint = _voiceTapTrailingConstraint;
@synthesize doodleHeightConstraint = _doodleHeightConstraint;
@synthesize doodleTopMarginConstraint = _doodleTopMarginConstraint;
@synthesize fakeOmniboxWidthConstraint = _fakeOmniboxWidthConstraint;
@synthesize fakeOmniboxHeightConstraint = _fakeOmniboxHeightConstraint;
@synthesize fakeOmniboxTopMarginConstraint = _fakeOmniboxTopMarginConstraint;
@synthesize fakeOmniboxBorder = _fakeOmniboxBorder;
@synthesize fakeOmniboxShadow = _fakeOmniboxShadow;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize isShowing = _isShowing;
@synthesize promoCanShow = _promoCanShow;
@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;

#pragma mark - ContentSuggestionsHeaderControlling

- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  // The scroll offset at which point the fake omnibox's frame should stop
  // growing.
  CGFloat maxScaleOffset =
      self.view.frame.size.height - ntp_header::kMinHeaderHeight;
  // The scroll offset at which point the fake omnibox's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = MIN(1, MAX(0, animatingOffset / ntp_header::kAnimationDistance));
  }

  CGFloat searchFieldNormalWidth =
      content_suggestions::searchFieldWidth(self.view.bounds.size.width);

  // Calculate the amount to grow the width and height of the fake omnibox so
  // that its frame covers the entire toolbar area.
  CGFloat maxXInset = ui::AlignValueToUpperPixel(
      (searchFieldNormalWidth - self.view.bounds.size.width) / 2 - 1);
  CGFloat maxHeightDiff =
      ntp_header::kToolbarHeight - content_suggestions::kSearchFieldHeight;

  self.fakeOmniboxWidthConstraint.constant =
      searchFieldNormalWidth - 2 * maxXInset * percent;
  self.fakeOmniboxTopMarginConstraint.constant =
      content_suggestions::searchFieldTopMargin() +
      ntp_header::kMaxTopMarginDiff * percent;
  self.fakeOmniboxHeightConstraint.constant =
      content_suggestions::kSearchFieldHeight + maxHeightDiff * percent;

  [self.fakeOmniboxBorder setAlpha:(1 - percent)];
  [self.fakeOmniboxShadow setAlpha:percent];

  // Adjust the position of the fake omnibox's subviews by adjusting their
  // constraint constant value.
  CGFloat constantDiff = percent * ntp_header::kMaxHorizontalMarginDiff;
  self.hintLabelLeadingConstraint.constant =
      constantDiff + ntp_header::kHintLabelSidePadding;
  self.voiceTapTrailingConstraint.constant = -constantDiff;
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  self.fakeOmniboxWidthConstraint.constant =
      content_suggestions::searchFieldWidth(width);
}

- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    // TODO(crbug.com/740793): Remove alert once the protocol to send commands
    // to the toolbar is implemented.
    [self showAlert:@"Cancel omnibox edit"];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)layoutHeader {
  [self.view layoutIfNeeded];
}

- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.logoVendor.showingLogo, self.promoCanShow, NO);
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(self.logoVendor.showingLogo,
                                                  self.promoCanShow, NO);
}

#pragma mark - ChromeBroadcastObserver

- (void)broadcastSelectedNTPPanel:(ntp_home::PanelIdentifier)panelIdentifier {
  self.isShowing = panelIdentifier == ntp_home::HOME_PANEL;
}

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  if (!self.fakeOmnibox) {
    [self addFakeOmnibox];

    self.logoVendor.view.isAccessibilityElement = YES;
    self.logoVendor.view.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_NEW_TAB_LOGO_ACCESSIBILITY_LABEL);

    [self.view addSubview:self.logoVendor.view];
    [self.view addSubview:self.fakeOmnibox];
    self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::searchFieldWidth(width)];
    [self addConstraintsForLogoView:self.logoVendor.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.view];

    [self addViewsToSearchField:self.fakeOmnibox];
    [self.logoVendor fetchDoodle];
  }
  return self.view;
}

#pragma mark - GoogleLandingConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  if (self.logoVendor.showingLogo != logoIsShowing) {
    self.logoVendor.showingLogo = logoIsShowing;
    [self.doodleHeightConstraint
        setConstant:content_suggestions::doodleHeight(logoIsShowing)];
    if (IsIPadIdiom())
      [self.fakeOmnibox setHidden:!logoIsShowing];
    [self.collectionSynchronizer invalidateLayout];
  }
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

  if (IsIPadIdiom())
    return;

  self.fakeOmnibox.hidden = NO;
  // TODO(crbug.com/740793): Remove alert once the protocol to send commands
  // to the toolbar is implemented.
  [self showAlert:@"Omnibox unfocused"];
  [self.collectionSynchronizer shiftTilesDown];
  [self.commandHandler dismissModals];
}

- (void)locationBarBecomesFirstResponder {
  if (!self.isShowing)
    return;

  self.omniboxFocused = YES;
  void (^completionBlock)() = ^{
    if (IsIPadIdiom())
      return;

    // TODO(crbug.com/740793): Remove alert once the protocol to send commands
    // to the toolbar is implemented.
    [self showAlert:@"Omnibox animation completed"];
    [UIView animateWithDuration:ios::material::kDuration1
                     animations:^{
                       [self.fakeOmniboxShadow setAlpha:0];
                     }];
    [self.fakeOmnibox setHidden:YES];
  };
  [self.collectionSynchronizer shiftTilesUpWithCompletionBlock:completionBlock];
}

#pragma mark - Private

// Initialize and add a search field tap target and a voice search button.
- (void)addFakeOmnibox {
  self.fakeOmnibox = [[UIButton alloc] init];
  if (IsIPadIdiom()) {
    UIImage* searchBoxImage = [[UIImage imageNamed:@"ntp_google_search_box"]
        resizableImageWithCapInsets:kSearchBoxStretchInsets];
    [self.fakeOmnibox setBackgroundImage:searchBoxImage
                                forState:UIControlStateNormal];
  }
  [self.fakeOmnibox setAdjustsImageWhenHighlighted:NO];

  // Set isAccessibilityElement to NO so that Voice Search button is accessible.
  [self.fakeOmnibox setIsAccessibilityElement:NO];
  self.fakeOmnibox.accessibilityIdentifier =
      ntp_home::FakeOmniboxAccessibilityID();

  // Set up fakebox hint label.
  UILabel* searchHintLabel = [[UILabel alloc] init];
  content_suggestions::configureSearchHintLabel(searchHintLabel,
                                                self.fakeOmnibox);

  self.hintLabelLeadingConstraint = [searchHintLabel.leadingAnchor
      constraintEqualToAnchor:[self.fakeOmnibox leadingAnchor]
                     constant:ntp_header::kHintLabelSidePadding];
  [_hintLabelLeadingConstraint setActive:YES];

  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  searchHintLabel.isAccessibilityElement = NO;
  UIButton* accessibilityButton = [[UIButton alloc] init];
  [accessibilityButton addTarget:self
                          action:@selector(fakeOmniboxTapped:)
                forControlEvents:UIControlEventTouchUpInside];
  accessibilityButton.isAccessibilityElement = YES;
  accessibilityButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self.fakeOmnibox addSubview:accessibilityButton];
  accessibilityButton.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.fakeOmnibox, accessibilityButton);

  // Add a voice search button.
  UIButton* voiceTapTarget = [[UIButton alloc] init];
  content_suggestions::configureVoiceSearchButton(voiceTapTarget,
                                                  self.fakeOmnibox);

  self.voiceTapTrailingConstraint = [voiceTapTarget.trailingAnchor
      constraintEqualToAnchor:[self.fakeOmnibox trailingAnchor]];
  [NSLayoutConstraint activateConstraints:@[
    [searchHintLabel.trailingAnchor
        constraintEqualToAnchor:voiceTapTarget.leadingAnchor],
    _voiceTapTrailingConstraint
  ]];

  if (self.voiceSearchIsEnabled) {
    [voiceTapTarget addTarget:self
                       action:@selector(loadVoiceSearch:)
             forControlEvents:UIControlEventTouchUpInside];
    [voiceTapTarget addTarget:self
                       action:@selector(preloadVoiceSearch:)
             forControlEvents:UIControlEventTouchDown];
  } else {
    [voiceTapTarget setEnabled:NO];
  }
}

- (void)loadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(
      base::UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  // TODO(crbug.com/740793): Remove alert once VoiceSearch is implemented.
  [self showAlert:@"Load VoiceSearch"];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  // TODO(crbug.com/740793): Remove alert once VoiceSearch is implemented.
  [self showAlert:@"Preload VoiceSearch"];
}

- (void)fakeOmniboxTapped:(id)sender {
  // TODO(crbug.com/740793): Remove alert once the protocol to send commands to
  // the toolbar is implemented.
  [self showAlert:@"Focus fakebox"];
}

// Adds the constraints for the |logoView|, the |fakeomnibox| related to the
// |headerView|. It also sets the properties constraints related to those views.
- (void)addConstraintsForLogoView:(UIView*)logoView
                      fakeOmnibox:(UIView*)fakeOmnibox
                    andHeaderView:(UIView*)headerView {
  self.doodleTopMarginConstraint = [logoView.topAnchor
      constraintEqualToAnchor:headerView.topAnchor
                     constant:content_suggestions::doodleTopMargin(NO)];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::doodleHeight(
                                    self.logoVendor.showingLogo)];
  self.fakeOmniboxHeightConstraint = [fakeOmnibox.heightAnchor
      constraintEqualToConstant:content_suggestions::kSearchFieldHeight];
  self.fakeOmniboxTopMarginConstraint = [fakeOmnibox.topAnchor
      constraintEqualToAnchor:logoView.bottomAnchor
                     constant:content_suggestions::searchFieldTopMargin()];
  [NSLayoutConstraint activateConstraints:@[
    self.doodleTopMarginConstraint,
    self.doodleHeightConstraint,
    self.fakeOmniboxWidthConstraint,
    self.fakeOmniboxHeightConstraint,
    self.fakeOmniboxTopMarginConstraint,
    [logoView.widthAnchor constraintEqualToAnchor:headerView.widthAnchor],
    [logoView.leadingAnchor constraintEqualToAnchor:headerView.leadingAnchor],
    [fakeOmnibox.centerXAnchor
        constraintEqualToAnchor:headerView.centerXAnchor],
  ]];
}

- (void)addViewsToSearchField:(UIView*)searchField {
  [searchField setBackgroundColor:[UIColor whiteColor]];
  UIImage* searchBorderImage =
      StretchableImageNamed(@"ntp_google_search_box", 12, 12);
  self.fakeOmniboxBorder =
      [[UIImageView alloc] initWithImage:searchBorderImage];
  [self.fakeOmniboxBorder setFrame:[searchField bounds]];
  [self.fakeOmniboxBorder setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                              UIViewAutoresizingFlexibleHeight];
  [searchField insertSubview:self.fakeOmniboxBorder atIndex:0];

  UIImage* fullBleedShadow = NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED);
  self.fakeOmniboxShadow = [[UIImageView alloc] initWithImage:fullBleedShadow];
  CGRect shadowFrame = [searchField bounds];
  shadowFrame.origin.y = searchField.bounds.size.height;
  shadowFrame.size.height = fullBleedShadow.size.height;
  [self.fakeOmniboxShadow setFrame:shadowFrame];
  [self.fakeOmniboxShadow setUserInteractionEnabled:NO];
  [self.fakeOmniboxShadow
      setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                          UIViewAutoresizingFlexibleTopMargin];
  [searchField addSubview:self.fakeOmniboxShadow];
  [self.fakeOmniboxShadow setAlpha:0];
}

// TODO(crbug.com/740793): Remove this method once no item is using it.
- (void)showAlert:(NSString*)title {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.parentViewController presentViewController:alertController
                                          animated:YES
                                        completion:nil];
}

@end
