// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"

#include "base/logging.h"
#include "components/google/core/browser/google_util.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sizes.
// Height of the header image at the top.
CGFloat kHeaderImageHeight = 144.;
// Size of the small icons next to the text.
CGFloat kIconSize = 16.;
// Separator line height.
CGFloat kSeparatorHeight = 2.;

// Font sizes
CGFloat kTitleFontSize = 23.;
CGFloat kTextFontSize = 12.;

// Horizontal margin between the container view and any elements inside.
CGFloat kHorizontalMargin = 16.;
// Horizontal margin between the small icon and the text next to it.
CGFloat kIconTextMargin = 16.;
// Horizontal margin on the left part of the separator.
CGFloat kLeftSeparatorMargin = kHorizontalMargin + kIconSize + kIconTextMargin;
// Vertical margin between texts.
CGFloat kVerticalBetweenTextMargin = 16.;
// Vertical margin above the first text and after the last text.
CGFloat kVerticalTextMargin = 32.;
// Vertical margin the main title and the identity picker.
CGFloat KTitlePickerMargin = 26.;

// Color displayed in the non-safe area.
const int kHeaderBackgroundColor = 0xf8f9fa;
// Alpha for the separator color.
const CGFloat kSeparatorColorAlpha = 0.12;
// Alpha for the title color.
const CGFloat kTitleColorAlpha = 0.87;
// Alpha for the text color.
const CGFloat kTextColorAlpha = 0.54;

const char* kSettingsSyncURL = "internal://settings-sync";
}  // namespace

@interface UnifiedConsentViewController ()<UIScrollViewDelegate> {
  std::vector<int> _consentStringIds;
}

// Read/write internal.
@property(nonatomic, readwrite) int openSettingsStringId;
// Main view.
@property(nonatomic, strong) UIScrollView* scrollView;
// Identity picker to change the identity to sign-in.
@property(nonatomic, strong) IdentityPickerView* identityPickerView;
// Vertical constraint on imageBackgroundView to have it over non-safe area.
@property(nonatomic, strong)
    NSLayoutConstraint* imageBackgroundViewHeightConstraint;
// Constraint when identityPickerView is hidden.
@property(nonatomic, strong) NSLayoutConstraint* noIdentityConstraint;
// Constraint when identityPickerView is visible.
@property(nonatomic, strong) NSLayoutConstraint* withIdentityConstraint;
// Settings link controller.
@property(nonatomic, strong) LabelLinkController* settingsLinkController;
// Label related to customize sync text.
@property(nonatomic, strong) UILabel* customizeSyncLabel;

@end

@implementation UnifiedConsentViewController

@synthesize delegate = _delegate;
@synthesize identityPickerView = _identityPickerView;
@synthesize imageBackgroundViewHeightConstraint =
    _imageBackgroundViewHeightConstraint;
@synthesize noIdentityConstraint = _noIdentityConstraint;
@synthesize openSettingsStringId = _openSettingsStringId;
@synthesize scrollView = _scrollView;
@synthesize settingsLinkController = _settingsLinkController;
@synthesize withIdentityConstraint = _withIdentityConstraint;
@synthesize customizeSyncLabel = _customizeSyncLabel;

- (const std::vector<int>&)consentStringIds {
  return _consentStringIds;
}

- (void)updateIdentityPickerViewWithUserFullName:(NSString*)fullName
                                           email:(NSString*)email {
  DCHECK(email);
  self.identityPickerView.hidden = NO;
  self.noIdentityConstraint.active = NO;
  self.withIdentityConstraint.active = YES;
  [self.identityPickerView setIdentityName:fullName email:email];
  [self setSettingsLinkURLShown:YES];
}

- (void)updateIdentityPickerViewWithAvatar:(UIImage*)avatar {
  DCHECK(!self.identityPickerView.hidden);
  [self.identityPickerView setIdentityAvatar:avatar];
}

- (void)hideIdentityPickerView {
  self.identityPickerView.hidden = YES;
  self.withIdentityConstraint.active = NO;
  self.noIdentityConstraint.active = YES;
  [self setSettingsLinkURLShown:NO];
}

- (void)scrollToBottom {
  CGPoint bottomOffset =
      CGPointMake(0, self.scrollView.contentSize.height -
                         self.scrollView.bounds.size.height +
                         self.scrollView.contentInset.bottom);
  [self.scrollView setContentOffset:bottomOffset animated:YES];
}

- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      self.scrollView.contentOffset.y + self.scrollView.frame.size.height;
  CGFloat scrollLimit =
      self.scrollView.contentSize.height + self.scrollView.contentInset.bottom;
  return scrollPosition >= scrollLimit;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Main scroll view.
  self.scrollView = [[UIScrollView alloc] initWithFrame:self.view.bounds];
  self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  if (@available(iOS 11, *)) {
    // The observed behavior was buggy. When the view appears on the screen,
    // the scrollvie was not scrolled all the way to the top. Adjusting the
    // safe area manually fixes the issue.
    self.scrollView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
  [self.view addSubview:self.scrollView];

  // Scroll view container.
  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollView addSubview:container];

  // View used to draw the background color of the header image, in the non-safe
  // areas (like the status bar).
  UIView* imageBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
  imageBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  imageBackgroundView.backgroundColor = UIColorFromRGB(kHeaderBackgroundColor);
  [container addSubview:imageBackgroundView];

  // Header image.
  UIImageView* headerImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/827072): Needs the real image.
  headerImageView.backgroundColor = [UIColor redColor];
  [container addSubview:headerImageView];

  // Title.
  UILabel* title = [[UILabel alloc] initWithFrame:CGRectZero];
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.text = l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE);
  _consentStringIds.push_back(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE);
  title.textColor = [UIColor colorWithWhite:0 alpha:kTitleColorAlpha];
  title.font = [UIFont systemFontOfSize:kTitleFontSize];
  title.numberOfLines = 0;

  [container addSubview:title];
  // Identity picker view.
  self.identityPickerView =
      [[IdentityPickerView alloc] initWithFrame:CGRectZero];
  self.identityPickerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.identityPickerView addTarget:self
                              action:@selector(identityPickerAction:)
                    forControlEvents:UIControlEventTouchUpInside];
  [container addSubview:self.identityPickerView];

  // Sync bookmark label.
  UILabel* syncBookmarkLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_DATA
                        withIcon:nil
          iconVerticallyCentered:YES
                      parentView:container];
  // Get more personalized label.
  UILabel* morePersonalizedLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_PERSONALIZED
                        withIcon:nil
          iconVerticallyCentered:YES
                      parentView:container];
  // Powerful Google label.
  UILabel* powerfulGoogleLabel =
      [self addLabelWithStringId:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_BETTER_BROWSER
                        withIcon:nil
          iconVerticallyCentered:YES
                      parentView:container];
  // Separator.
  UIView* separator = [[UIView alloc] initWithFrame:CGRectZero];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor =
      [UIColor colorWithWhite:0 alpha:kSeparatorColorAlpha];
  [container addSubview:separator];
  // Customize label.
  self.openSettingsStringId = IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS;
  self.customizeSyncLabel = [self addLabelWithStringId:self.openSettingsStringId
                                              withIcon:nil
                                iconVerticallyCentered:NO
                                            parentView:container];

  // Layouts
  NSDictionary* views = @{
    @"header" : headerImageView,
    @"title" : title,
    @"picker" : self.identityPickerView,
    @"container" : container,
    @"scrollview" : self.scrollView,
    @"separator" : separator,
    @"synctext" : syncBookmarkLabel,
    @"personalizedtext" : morePersonalizedLabel,
    @"powerfultext" : powerfulGoogleLabel,
    @"customizesynctext" : self.customizeSyncLabel,
  };
  NSDictionary* metrics = @{
    @"TitlePickerMargin" : @(KTitlePickerMargin),
    @"HMargin" : @(kHorizontalMargin),
    @"VBetweenText" : @(kVerticalBetweenTextMargin),
    @"LeftSeparMrg" : @(kLeftSeparatorMargin),
    @"VTextMargin" : @(kVerticalTextMargin),
    @"SeparatorHeight" : @(kSeparatorHeight),
    @"HeaderHeight" : @(kHeaderImageHeight),
  };
  NSArray* constraints = @[
    // Horitizontal constraints.
    @"H:|[scrollview]|",
    @"H:|[container]|",
    @"H:|[header]|",
    @"H:|-(HMargin)-[title]-(HMargin)-|",
    @"H:|-(HMargin)-[picker]-(HMargin)-|",
    @"H:|-(LeftSeparMrg)-[separator]-(HMargin)-|",
    // Vertical constraints.
    @"V:|[scrollview]|",
    @"V:|[container]|",
    @"V:|[header][title]-(TitlePickerMargin)-[picker]",
    @"V:[synctext]-(VBetweenText)-[personalizedtext]",
    @"V:[personalizedtext]-(VBetweenText)-[powerfultext]",
    @"V:[powerfultext]-(VTextMargin)-[separator]",
    @"V:[separator]-(VBetweenText)-[customizesynctext]-(VTextMargin)-|",
    // Size constraints.
    @"V:[header(HeaderHeight)]",
    @"V:[separator(SeparatorHeight)]",
  ];
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

  // Adding constraints with or without identity.
  self.noIdentityConstraint =
      [syncBookmarkLabel.topAnchor constraintEqualToAnchor:title.bottomAnchor
                                                  constant:kVerticalTextMargin];
  self.withIdentityConstraint = [syncBookmarkLabel.topAnchor
      constraintEqualToAnchor:self.identityPickerView.bottomAnchor
                     constant:kVerticalTextMargin];
  // Adding constraints for the container.
  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(self.view);
  [container.widthAnchor constraintEqualToAnchor:safeArea.widthAnchor].active =
      YES;
  // Adding constraints for |imageBackgroundView|.
  AddSameCenterXConstraint(self.view, imageBackgroundView);
  [imageBackgroundView.widthAnchor
      constraintEqualToAnchor:self.view.widthAnchor]
      .active = YES;
  self.imageBackgroundViewHeightConstraint = [imageBackgroundView.heightAnchor
      constraintEqualToAnchor:headerImageView.heightAnchor];
  self.imageBackgroundViewHeightConstraint.active = YES;
  [imageBackgroundView.bottomAnchor
      constraintEqualToAnchor:headerImageView.bottomAnchor]
      .active = YES;

  // Update UI.
  [self hideIdentityPickerView];
  [self updateScrollViewAndImageBackgroundView];
}

- (void)viewSafeAreaInsetsDidChange {
  // Updates the scroll view content inset, used by iOS 11 or later.
  [super viewSafeAreaInsetsDidChange];
  [self updateScrollViewAndImageBackgroundView];
}

// Updates the scroll view content inset, used by pre iOS 11.
- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self updateScrollViewAndImageBackgroundView];
      }
                      completion:nil];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent)
    return;
  [parent.view layoutIfNeeded];
  // Needs to add the scroll view delegate only when all the view layouts are
  // fully done.
  dispatch_async(dispatch_get_main_queue(), ^{
    // Having a layout of the parent view makes the scroll view not being
    // presented at the top. Scrolling to the top is required.
    CGPoint topOffset = CGPointMake(0, -self.scrollView.contentInset.top);
    [self.scrollView setContentOffset:topOffset animated:NO];
    self.scrollView.delegate = self;
    [self sendDidReachBottomIfReached];
  });
}

#pragma mark - UI actions

- (void)identityPickerAction:(id)sender {
  [self.delegate unifiedConsentViewControllerDidTapIdentityPickerView:self];
}

#pragma mark - Private

// Adds an icon and a label, into |parentView|, next to each other with the
// constraints between the icon and the label.
// If |iconVerticallyCentered| is true, the |icon| is vertically centered with
// the label. Otherwise, it the |icon| is vertically aligned with the first line
// of the label.
- (UILabel*)addLabelWithStringId:(int)stringId
                        withIcon:(UIImage*)icon
          iconVerticallyCentered:(BOOL)iconVerticallyCentered
                      parentView:(UIView*)parentView {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont systemFontOfSize:kTextFontSize];
  label.text = l10n_util::GetNSString(stringId);
  _consentStringIds.push_back(stringId);
  label.textColor = [UIColor colorWithWhite:0 alpha:kTextColorAlpha];
  label.numberOfLines = 0;
  [parentView addSubview:label];
  UIImageView* image = [[UIImageView alloc] initWithFrame:CGRectZero];
  if (icon) {
    image.image = icon;
  } else {
    // TODO(crbug.com/827072): Remove this once the bug will be fixed, and add
    // DCHECK(icon);
    image.backgroundColor = [UIColor redColor];
  }
  image.translatesAutoresizingMaskIntoConstraints = NO;
  [parentView addSubview:image];
  NSDictionary* views = NSDictionaryOfVariableBindings(label, image);
  NSArray* constraints = @[
    @"H:|-(HMargin)-[image]-(IconTextMargin)-[label]-(HMargin)-|",
    @"H:[image(IconSize)]",
    @"V:[image(IconSize)]",
  ];
  NSDictionary* metrics = @{
    @"HMargin" : @(kHorizontalMargin),
    @"IconSize" : @(kIconSize),
    @"IconTextMargin" : @(kIconTextMargin),
  };
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);
  if (iconVerticallyCentered) {
    AddSameCenterYConstraint(label, image);
  } else {
    // |icon| should be aligned to first line of |label|. This has to be done
    // according to the middle of the cap height of the font.
    // The cap height position is based on the ascender.
    UIFont* font = label.font;
    [image.centerYAnchor
        constraintEqualToAnchor:label.topAnchor
                       constant:font.ascender - font.capHeight +
                                font.capHeight / 2.]
        .active = YES;
  }
  return label;
}

// Adds or removes the Settings link in |self.customizeSyncLabel|.
- (void)setSettingsLinkURLShown:(BOOL)showLink {
  self.customizeSyncLabel.text =
      l10n_util::GetNSString(self.openSettingsStringId);
  GURL URL = google_util::AppendGoogleLocaleParam(
      GURL(kSettingsSyncURL), GetApplicationContext()->GetApplicationLocale());
  NSRange range;
  NSString* text = self.customizeSyncLabel.text;
  self.customizeSyncLabel.text = ParseStringWithLink(text, &range);
  DCHECK(range.location != NSNotFound && range.length != 0);
  if (!showLink) {
    self.settingsLinkController = nil;
  } else {
    __weak UnifiedConsentViewController* weakSelf = self;
    self.settingsLinkController =
        [[LabelLinkController alloc] initWithLabel:self.customizeSyncLabel
                                            action:^(const GURL& URL) {
                                              [weakSelf openSettings];
                                            }];
    [self.settingsLinkController
        setLinkColor:[[MDCPalette cr_bluePalette] tint500]];
    [self.settingsLinkController addLinkWithRange:range url:URL];
  }
}

// Updates constraints and content insets for the |scrollView| and
// |imageBackgroundView| related to non-safe area.
- (void)updateScrollViewAndImageBackgroundView {
  if (@available(iOS 11, *)) {
    self.scrollView.contentInset = self.view.safeAreaInsets;
    self.imageBackgroundViewHeightConstraint.constant =
        self.view.safeAreaInsets.top;
  } else {
    CGFloat statusBarHeight =
        [UIApplication sharedApplication].isStatusBarHidden ? 0.
                                                            : StatusBarHeight();
    self.scrollView.contentInset = UIEdgeInsetsMake(statusBarHeight, 0, 0, 0);
    self.imageBackgroundViewHeightConstraint.constant = statusBarHeight;
  }
  if (self.scrollView.delegate == self) {
    // Don't send the notification if the delegate is not configured yet.
    [self sendDidReachBottomIfReached];
  }
}

// Notifies |delegate| that the user tapped on "Settings" link.
- (void)openSettings {
  [self.delegate unifiedConsentViewControllerDidTapSettingsLink:self];
}

// Sends notification to the delegate if the scroll view is scrolled to the
// bottom.
- (void)sendDidReachBottomIfReached {
  if (self.isScrolledToBottom) {
    [self.delegate unifiedConsentViewControllerDidReachBottom:self];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollView, scrollView);
  [self sendDidReachBottomIfReached];
}

@end
