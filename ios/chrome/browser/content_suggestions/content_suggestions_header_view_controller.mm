// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_header_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
const UIEdgeInsets kSearchBoxStretchInsets = {3, 3, 3, 3};
const CGFloat kHintLabelSidePadding = 12;
}  // namespace

@interface ContentSuggestionsHeaderViewController ()

// |YES| when notifications indicate the omnibox is focused.
@property(nonatomic, assign, getter=isOmniboxFocused) BOOL omniboxFocused;

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// |YES| if a what's new promo can be displayed.
@property(nonatomic, assign) BOOL promoCanShow;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

// |YES| if the google landing toolbar can show the forward arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoForward;

// |YES| if the google landing toolbar can show the back arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoBack;

// Gets the icon of a what's new promo.
// TODO(crbug.com/694750): This should not be WhatsNewIcon.
@property(nonatomic, assign) WhatsNewIcon promoIcon;

// Gets the text of a what's new promo.
@property(nonatomic, copy) NSString* promoText;

// The number of tabs to show in the google landing fake toolbar.
@property(nonatomic, assign) int tabCount;

@property(nonatomic, strong) NewTabPageHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* voiceTapTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, assign) BOOL logoFetched;

@end

@implementation ContentSuggestionsHeaderViewController

@synthesize dispatcher = _dispatcher;
@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize readingListModel = _readingListModel;

@synthesize logoVendor = _logoVendor;
@synthesize promoCanShow = _promoCanShow;
@synthesize canGoForward = _canGoForward;
@synthesize canGoBack = _canGoBack;
@synthesize promoIcon = _promoIcon;
@synthesize promoText = _promoText;
@synthesize isShowing = _isShowing;
@synthesize omniboxFocused = _omniboxFocused;
@synthesize tabCount = _tabCount;

@synthesize headerView = _headerView;
@synthesize fakeOmnibox = _fakeOmnibox;
@synthesize hintLabelLeadingConstraint = _hintLabelLeadingConstraint;
@synthesize voiceTapTrailingConstraint = _voiceTapTrailingConstraint;
@synthesize doodleHeightConstraint = _doodleHeightConstraint;
@synthesize doodleTopMarginConstraint = _doodleTopMarginConstraint;
@synthesize fakeOmniboxWidthConstraint = _fakeOmniboxWidthConstraint;
@synthesize fakeOmniboxHeightConstraint = _fakeOmniboxHeightConstraint;
@synthesize fakeOmniboxTopMarginConstraint = _fakeOmniboxTopMarginConstraint;
@synthesize voiceSearchIsEnabled = _voiceSearchIsEnabled;
@synthesize logoIsShowing = _logoIsShowing;
@synthesize logoFetched = _logoFetched;

#pragma mark - Public

- (UIView*)toolBarView {
  return self.headerView.toolBarView;
}

#pragma mark - Property

- (void)setIsShowing:(BOOL)isShowing {
  _isShowing = isShowing;
  if (isShowing)
    [self.headerView hideToolbarViewsForNewTabPage];
}

#pragma mark - ContentSuggestionsHeaderControlling

- (void)updateFakeOmniboxForOffset:(CGFloat)offset width:(CGFloat)width {
  NSArray* constraints =
      @[ self.hintLabelLeadingConstraint, self.voiceTapTrailingConstraint ];

  [self.headerView updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                                   height:self.fakeOmniboxHeightConstraint
                                topMargin:self.fakeOmniboxTopMarginConstraint
                       subviewConstraints:constraints
                            logoIsShowing:self.logoIsShowing
                                forOffset:offset
                                    width:width];
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  self.fakeOmniboxWidthConstraint.constant =
      content_suggestions::searchFieldWidth(width);
}

- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    [self.dispatcher cancelOmniboxEdit];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)layoutHeader {
  [self.headerView layoutIfNeeded];
}

- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.logoIsShowing, self.promoCanShow, YES);
  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsIPadIdiom())
    offsetY -= ntp_header::kToolbarHeight;

  return offsetY;
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(self.logoIsShowing,
                                                  self.promoCanShow, YES);
}

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  if (!self.headerView) {
    self.headerView = [[NewTabPageHeaderView alloc] init];
    [self addFakeOmnibox];

    self.logoVendor.view.isAccessibilityElement = YES;
    self.logoVendor.view.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_NEW_TAB_LOGO_ACCESSIBILITY_LABEL);

    [self.headerView addSubview:self.logoVendor.view];
    [self.headerView addSubview:self.fakeOmnibox];
    self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::searchFieldWidth(width)];
    [self addConstraintsForLogoView:self.logoVendor.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.headerView];

    if (!IsIPadIdiom()) {
      // iPhone header also contains a toolbar since the normal toolbar is
      // hidden.
      [_headerView addToolbarWithReadingListModel:self.readingListModel
                                       dispatcher:self.dispatcher];
      [_headerView setToolbarTabCount:self.tabCount];
      [_headerView setCanGoForward:self.canGoForward];
      [_headerView setCanGoBack:self.canGoBack];
    }

    [self.headerView addViewsToSearchField:self.fakeOmnibox];
    [self.logoVendor fetchDoodle];
  }
  return self.headerView;
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
                     constant:kHintLabelSidePadding];
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
  [self.commandHandler dismissModals];

  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  UIView* view = base::mac::ObjCCastStrict<UIView>(sender);
  StartVoiceSearchCommand* command =
      [[StartVoiceSearchCommand alloc] initWithOriginView:view];
  [self.dispatcher startVoiceSearch:command];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  [self.dispatcher preloadVoiceSearch];
}

- (void)fakeOmniboxTapped:(id)sender {
  [self.dispatcher focusFakebox];
}

// If Google is not the default search engine, hide the logo, doodle and
// fakebox. Make them appear if Google is set as default.
- (void)updateLogoAndFakeboxDisplay {
  if (self.logoVendor.showingLogo != self.logoIsShowing) {
    self.logoVendor.showingLogo = self.logoIsShowing;
    [self.doodleHeightConstraint
        setConstant:content_suggestions::doodleHeight(self.logoIsShowing)];
    if (IsIPadIdiom())
      [self.fakeOmnibox setHidden:!self.logoIsShowing];
    [self.collectionSynchronizer invalidateLayout];
  }
}

// Adds the constraints for the |logoView|, the |fakeomnibox| related to the
// |headerView|. It also sets the properties constraints related to those views.
- (void)addConstraintsForLogoView:(UIView*)logoView
                      fakeOmnibox:(UIView*)fakeOmnibox
                    andHeaderView:(UIView*)headerView {
  self.doodleTopMarginConstraint = [logoView.topAnchor
      constraintEqualToAnchor:headerView.topAnchor
                     constant:content_suggestions::doodleTopMargin(YES)];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::doodleHeight(
                                    self.logoIsShowing)];
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

- (void)shiftTilesDown {
  if (!IsIPadIdiom()) {
    self.fakeOmnibox.hidden = NO;
    [self.dispatcher onFakeboxBlur];
  }

  [self.collectionSynchronizer shiftTilesDown];

  [self.commandHandler dismissModals];
}

- (void)shiftTilesUp {
  void (^completionBlock)() = ^{
    if (!IsIPadIdiom()) {
      [self.dispatcher onFakeboxAnimationComplete];
      [self.headerView fadeOutShadow];
      [self.fakeOmnibox setHidden:YES];
    }
  };
  [self.collectionSynchronizer shiftTilesUpWithCompletionBlock:completionBlock];
}

#pragma mark - ToolbarOwner

- (ToolbarController*)relinquishedToolbarController {
  return [self.headerView relinquishedToolbarController];
}

- (void)reparentToolbarController {
  [self.headerView reparentToolbarController];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.logoVendor logoAnimationControllerOwner];
}

#pragma mark - GoogleLandingConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  [self updateLogoAndFakeboxDisplay];
}

- (void)setMaximumMostVisitedSitesShown:
    (NSUInteger)maximumMostVisitedSitesShown {
}

- (void)mostVisitedDataUpdated {
  // Do nothing as it is handled in the ContentSuggestionsMediator.
}

- (void)mostVisitedIconMadeAvailableAtIndex:(NSUInteger)index {
  // Do nothing as it is handled in the ContentSuggestionsMediator.
}

- (void)setTabCount:(int)tabCount {
  _tabCount = tabCount;
  [self.headerView setToolbarTabCount:tabCount];
}

- (void)setCanGoForward:(BOOL)canGoForward {
  _canGoForward = canGoForward;
  [self.headerView setCanGoForward:self.canGoForward];
}

- (void)setCanGoBack:(BOOL)canGoBack {
  _canGoBack = canGoBack;
  [self.headerView setCanGoBack:self.canGoBack];
}

- (void)locationBarBecomesFirstResponder {
  if (!self.isShowing)
    return;

  self.omniboxFocused = YES;
  [self shiftTilesUp];
}

- (void)locationBarResignsFirstResponder {
  if (!self.isShowing && ![self.delegate isScrolledToTop])
    return;

  self.omniboxFocused = NO;
  if ([self.delegate isContextMenuVisible]) {
    return;
  }

  [self shiftTilesDown];
}

@end
