
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/tab_view.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_delegate.h"
#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_interaction.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/highlight_button.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "third_party/google_toolbox_for_mac/src/iPhone/GTMFadeTruncatingLabel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#import "ui/gfx/ios/uikit_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tab close button insets.
const CGFloat kTabCloseTopInset = -1.0;
const CGFloat kTabCloseLeftInset = 0.0;
const CGFloat kTabCloseBottomInset = 0.0;
const CGFloat kTabCloseRightInset = 0.0;
const CGFloat kTabBackgroundLeftCapInset = 24.0;
const CGFloat kFaviconLeftInset = 23.5;
const CGFloat kFaviconVerticalOffset = 2.0;
const CGFloat kTabStripLineMargin = 2.5;
const CGFloat kTabStripLineHeight = 0.5;
const CGFloat kCloseButtonHorizontalShift = 19;
const CGFloat kCloseButtonVerticalShift = 4.0;
const CGFloat kTitleLeftMargin = 8.0;
const CGFloat kTitleRightMargin = 0.0;

const CGFloat kCloseButtonSize = 24.0;
const CGFloat kFaviconSize = 16.0;

const int kTabCloseTint = 0x3C4043;
const int kTabCloseTintIncognito = 0xFFFFFF;
}

@interface TabView ()<DropAndNavigateDelegate> {
  __weak id<TabViewDelegate> _delegate;

  // Close button for this tab.
  UIButton* _closeButton;

  // View that draws the tab title.
  GTMFadeTruncatingLabel* _titleLabel;

  // Background image for this tab.
  UIImageView* _backgroundImageView;
  // This view is used to draw a separator line at the bottom of the tab view.
  // This view is hidden when the tab view is in a selected state.
  UIView* _lineSeparator;
  BOOL _incognitoStyle;

  // Set to YES when the layout constraints have been initialized.
  BOOL _layoutConstraintsInitialized;

  // Image view used to draw the favicon and spinner.
  UIImageView* _faviconView;

  // If |YES|, this view will adjust its appearance and draw as a collapsed tab.
  BOOL _collapsed;

  MDCActivityIndicator* _activityIndicator;

  API_AVAILABLE(ios(11.0)) DropAndNavigateInteraction* _dropInteraction;
}
@end

@interface TabView (Private)

// Creates the close button, favicon button, and title.
- (void)createButtonsAndLabel;

// Updates this tab's line separator color based on the current incognito style.
- (void)updateLineSeparator;

// Updates this tab's background image based on the value of |selected|.
- (void)updateBackgroundImage:(BOOL)selected;

// Updates this tab's close button image based on the current incognito style.
- (void)updateCloseButtonImages;

// Return the default favicon image based on the current incognito style.
- (UIImage*)defaultFaviconImage;

// Returns the rect in which to draw the favicon.
- (CGRect)faviconRectForBounds:(CGRect)bounds;

// Returns the rect in which to draw the tab title.
- (CGRect)titleRectForBounds:(CGRect)bounds;

// Returns the frame rect for the close button.
- (CGRect)closeRectForBounds:(CGRect)bounds;

@end

@implementation TabView

@synthesize delegate = _delegate;
@synthesize titleLabel = _titleLabel;
@synthesize collapsed = _collapsed;
@synthesize background = background_;
@synthesize incognitoStyle = _incognitoStyle;

- (id)initWithEmptyView:(BOOL)emptyView selected:(BOOL)selected {
  if ((self = [super initWithFrame:CGRectZero])) {
    [self setOpaque:NO];
    [self createCommonViews];
    // -setSelected only calls -updateBackgroundImage if the selected state
    // changes.  |isSelected| defaults to NO, so if |selected| is also NO,
    // -updateBackgroundImage needs to be called explicitly.
    [self setSelected:selected];
    [self updateLineSeparator];
    [self updateBackgroundImage:selected];
    if (!emptyView)
      [self createButtonsAndLabel];

    [self addTarget:self
                  action:@selector(tabWasTapped)
        forControlEvents:UIControlEventTouchUpInside];

    if (DragAndDropIsEnabled()) {
      _dropInteraction =
          [[DropAndNavigateInteraction alloc] initWithDelegate:self];
      [self addInteraction:_dropInteraction];
    }
  }
  return self;
}

- (void)setSelected:(BOOL)selected {
  BOOL wasSelected = [self isSelected];
  [super setSelected:selected];

  [_lineSeparator setHidden:selected];

  if (selected != wasSelected)
    [self updateBackgroundImage:selected];

  // It would make more sense to set active/inactive on tab_view itself, but
  // tab_view is not an an accessible element, and making it one would add
  // several complicated layers to UIA.  Instead, simply set active/inactive
  // here to be used by UIA.
  [_titleLabel setAccessibilityValue:(selected ? @"active" : @"inactive")];
}

- (void)setCollapsed:(BOOL)collapsed {
  if (_collapsed != collapsed)
    [_closeButton setHidden:collapsed];

  _collapsed = collapsed;
}

- (void)setTitle:(NSString*)title {
  if ([_titleLabel.text isEqualToString:title])
    return;
  if (base::i18n::GetStringDirection(base::SysNSStringToUTF16(title)) ==
      base::i18n::RIGHT_TO_LEFT) {
    [_titleLabel setTruncateMode:GTMFadeTruncatingHead];
  } else {
    [_titleLabel setTruncateMode:GTMFadeTruncatingTail];
  }
  _titleLabel.text = title;
  [_closeButton setAccessibilityValue:title];
}

- (UIImage*)favicon {
  return [_faviconView image];
}

- (void)setFavicon:(UIImage*)favicon {
  if (!favicon)
    favicon = [self defaultFaviconImage];
  [_faviconView setImage:favicon];
}

- (void)setIncognitoStyle:(BOOL)incognitoStyle {
  _incognitoStyle = incognitoStyle;
  _titleLabel.textColor =
      incognitoStyle ? [UIColor whiteColor] : [UIColor blackColor];
  [_faviconView setImage:[self defaultFaviconImage]];
  [self updateLineSeparator];
  [self updateCloseButtonImages];
  [self updateBackgroundImage:[self isSelected]];
}

- (void)startProgressSpinner {
  [_activityIndicator startAnimating];
  [_activityIndicator setHidden:NO];
  [_faviconView setHidden:YES];
}

- (void)stopProgressSpinner {
  [_activityIndicator stopAnimating];
  [_activityIndicator setHidden:YES];
  [_faviconView setHidden:NO];
}

#pragma mark - UIView overrides

- (void)setFrame:(CGRect)frame {
  const CGRect previousFrame = [self frame];
  [super setFrame:frame];
  // We are checking for a zero frame before triggering constraints updates in
  // order to prevent computation of constraints that will never be used for the
  // final layout. We could also initialize with a dummy frame but first this is
  // inefficient and second it's non trivial to compute the minimum valid frame
  // in regard to tweakable constants.
  if (CGRectEqualToRect(CGRectZero, previousFrame) &&
      !_layoutConstraintsInitialized) {
    [self setNeedsUpdateConstraints];
  }
}

- (void)updateConstraints {
  [super updateConstraints];
  if (!_layoutConstraintsInitialized &&
      !CGRectEqualToRect(CGRectZero, self.frame)) {
    _layoutConstraintsInitialized = YES;
    [self addCommonConstraints];
    // Add buttons and labels constraints if needed.
    if (_closeButton)
      [self addButtonsAndLabelConstraints];
  }
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Account for the trapezoidal shape of the tab.  Inset the tab bounds by
  // (y = -2.2x + 56), determined empirically from looking at the tab background
  // images.
  CGFloat inset = MAX(0.0, (point.y - 56) / -2.2);
  return CGRectContainsPoint(CGRectInset([self bounds], inset, 0), point);
}

#pragma mark - Private

- (void)createCommonViews {
  _backgroundImageView = [[UIImageView alloc] init];
  [_backgroundImageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_backgroundImageView];

  _lineSeparator = [[UIView alloc] initWithFrame:CGRectZero];
  [_lineSeparator setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self addSubview:_lineSeparator];
}

- (void)addCommonConstraints {
  NSDictionary* commonViewsDictionary = @{
    @"backgroundImageView" : _backgroundImageView,
    @"lineSeparator" : _lineSeparator
  };
  NSArray* commonConstraints = @[
    @"H:|-0-[backgroundImageView]-0-|",
    @"V:|-0-[backgroundImageView]-0-|",
    @"H:|-tabStripLineMargin-[lineSeparator]-tabStripLineMargin-|",
    @"V:[lineSeparator(==tabStripLineHeight)]-0-|",
  ];
  NSDictionary* commonMetrics = @{
    @"tabStripLineMargin" : @(kTabStripLineMargin),
    @"tabStripLineHeight" : @(kTabStripLineHeight)
  };
  ApplyVisualConstraintsWithMetrics(commonConstraints, commonViewsDictionary,
                                    commonMetrics);
}

- (void)createButtonsAndLabel {
  _closeButton = [HighlightButton buttonWithType:UIButtonTypeCustom];
  [_closeButton setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_closeButton setContentEdgeInsets:UIEdgeInsetsMake(kTabCloseTopInset,
                                                      kTabCloseLeftInset,
                                                      kTabCloseBottomInset,
                                                      kTabCloseRightInset)];
  [_closeButton setAccessibilityLabel:l10n_util::GetNSString(
                                          IDS_IOS_TOOLS_MENU_CLOSE_TAB)];
  [_closeButton addTarget:self
                   action:@selector(closeButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  [self addSubview:_closeButton];

  // Add fade truncating label.
  _titleLabel = [[GTMFadeTruncatingLabel alloc] initWithFrame:CGRectZero];
  [_titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_titleLabel setFont:[MDCTypography body1Font]];
  // Setting NSLineBreakByCharWrapping fixes an issue where the beginning of the
  // text is truncated for RTL text writing direction. Anyway since the label is
  // only one line and the end of the text is faded behind a gradient mask, it
  // is visually almost equivalent to NSLineBreakByClipping.
  [_titleLabel setLineBreakMode:NSLineBreakByCharWrapping];

  [_titleLabel setTextAlignment:NSTextAlignmentNatural];
  [self addSubview:_titleLabel];

  CGRect faviconFrame = CGRectMake(0, 0, kFaviconSize, kFaviconSize);
  _faviconView = [[UIImageView alloc] initWithFrame:faviconFrame];
  [_faviconView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_faviconView setContentMode:UIViewContentModeScaleAspectFit];
  [_faviconView setImage:[self defaultFaviconImage]];
  [_faviconView setAccessibilityIdentifier:@"Favicon"];
  [self addSubview:_faviconView];

  _activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:faviconFrame];
  [_activityIndicator setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_activityIndicator
      setCycleColors:@[ [[MDCPalette cr_bluePalette] tint500] ]];
  [_activityIndicator setRadius:ui::AlignValueToUpperPixel(kFaviconSize / 2)];
  [self addSubview:_activityIndicator];
}

- (void)addButtonsAndLabelConstraints {
  // Constraints on the Top bar, snapshot view, and shadow view.
  NSDictionary* viewsDictionary = @{
    @"close" : _closeButton,
    @"title" : _titleLabel,
    @"favicon" : _faviconView,
  };
  NSArray* constraints = @[
    @"H:|-faviconLeftInset-[favicon(faviconSize)]",
    @"V:|-faviconVerticalOffset-[favicon]-0-|",
    @"H:[close(==closeButtonSize)]-closeButtonHorizontalShift-|",
    @"V:|-closeButtonVerticalShift-[close]-0-|",
    @"H:[favicon]-titleLeftMargin-[title]-titleRightMargin-[close]",
    @"V:[title(==titleHeight)]",
  ];

  CGFloat faviconLeftInset = kFaviconLeftInset;
  CGFloat faviconVerticalOffset = kFaviconVerticalOffset;
  NSDictionary* metrics = @{
    @"closeButtonSize" : @(kCloseButtonSize),
    @"closeButtonHorizontalShift" : @(kCloseButtonHorizontalShift),
    @"closeButtonVerticalShift" : @(kCloseButtonVerticalShift),
    @"titleLeftMargin" : @(kTitleLeftMargin),
    @"titleRightMargin" : @(kTitleRightMargin),
    @"titleHeight" : @(kFaviconSize),
    @"faviconLeftInset" : @(AlignValueToPixel(faviconLeftInset)),
    @"faviconVerticalOffset" : @(faviconVerticalOffset),
    @"faviconSize" : @(kFaviconSize),
  };
  ApplyVisualConstraintsWithMetrics(constraints, viewsDictionary, metrics);
  AddSameCenterXConstraint(self, _faviconView, _activityIndicator);
  AddSameCenterYConstraint(self, _faviconView, _activityIndicator);
  AddSameCenterYConstraint(self, _faviconView, _titleLabel);
}

- (void)updateLineSeparator {
  UIColor* separatorColor =
      _incognitoStyle ? [UIColor colorWithWhite:36 / 255.0 alpha:1.0]
                      : [UIColor colorWithWhite:185 / 255.0 alpha:1.0];
  [_lineSeparator setBackgroundColor:separatorColor];
}

- (void)updateBackgroundImage:(BOOL)selected {
  NSString* state = (selected ? @"foreground" : @"background");
  NSString* incognito = _incognitoStyle ? @"incognito_" : @"";
  NSString* imageName =
      [NSString stringWithFormat:@"tabstrip_%@%@_tab", incognito, state];
  CGFloat leftInset = kTabBackgroundLeftCapInset;
  UIImage* backgroundImage =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], leftInset, 0);
  [_backgroundImageView setImage:backgroundImage];
}

- (void)updateCloseButtonImages {
  if (IsUIRefreshPhase1Enabled()) {
    [_closeButton
        setImage:[[UIImage imageNamed:@"grid_cell_close_button"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    _closeButton.tintColor = _incognitoStyle
                                 ? UIColorFromRGB(kTabCloseTintIncognito)
                                 : UIColorFromRGB(kTabCloseTint);
  } else {
    NSString* incognito = self.incognitoStyle ? @"_incognito" : @"";
    UIImage* normalImage = [UIImage
        imageNamed:[NSString stringWithFormat:@"tabstrip_tab_close%@_legacy",
                                              incognito]];
    UIImage* pressedImage = [UIImage
        imageNamed:[NSString
                       stringWithFormat:@"tabstrip_tab_close%@_pressed_legacy",
                                        incognito]];
    [_closeButton setImage:normalImage forState:UIControlStateNormal];
    [_closeButton setImage:pressedImage forState:UIControlStateHighlighted];
  }
}

- (UIImage*)defaultFaviconImage {
  return self.incognitoStyle ? [UIImage imageNamed:@"default_favicon_incognito"]
                             : [UIImage imageNamed:@"default_favicon"];
}

#pragma mark - DropAndNavigateDelegate

- (void)URLWasDropped:(GURL const&)url {
  [_delegate tabView:self receivedDroppedURL:url];
}

#pragma mark - Touch events

- (void)closeButtonPressed {
  [_delegate tabViewcloseButtonPressed:self];
}

- (void)tabWasTapped {
  [_delegate tabViewTapped:self];
}

@end
