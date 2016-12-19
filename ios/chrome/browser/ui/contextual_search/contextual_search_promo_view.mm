// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/contextual_search_promo_view.h"

#include "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
const int kMargin = 16;
const int kSpaceBelowText = 32;
const int kButtonSeparator = 8;
const int kButtonHeight = 36;
const int kButtonMinWidth = 88;
const int kDividerHeight = 1;

const int kTextFontSize = 16;
const int kButtonFontSize = 14;
const CGFloat kTextColorGrayShade = 0.494;
const int kLinkColorRGB = 0x5D9AFF;

const CGFloat kLineSpace = 1.15;

// Animation timings.
const CGFloat kCloseDuration = ios::material::kDuration1;
}

@interface ContextualSearchPromoView ()
@property(nonatomic, assign) id<ContextualSearchPromoViewDelegate> delegate;
@end

@implementation ContextualSearchPromoView {
  base::scoped_nsobject<LabelLinkController> _linkController;
  base::scoped_nsobject<NSMutableArray> _constraints;
}

@synthesize disabled = _disabled;
@synthesize delegate = _delegate;

+ (BOOL)requiresConstraintBasedLayout {
  return YES;
}

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<ContextualSearchPromoViewDelegate>)delegate {
  if (!(self = [super initWithFrame:frame]))
    return nil;

  self.delegate = delegate;
  self.backgroundColor = [UIColor colorWithWhite:0.93 alpha:1.0];

  // Temp array to accumulate constraints as views are created.
  NSMutableArray* constraints = [NSMutableArray array];

  // Initialize text label and its link controller.
  UILabel* text = [[[UILabel alloc] initWithFrame:CGRectZero] autorelease];
  base::WeakNSObject<ContextualSearchPromoView> weakSelf(self);
  _linkController.reset([[LabelLinkController alloc]
      initWithLabel:text
             action:^(const GURL& gurl) {
               [[weakSelf delegate] promoViewSettingsTapped];
             }]);
  [_linkController setLinkColor:UIColorFromRGB(kLinkColorRGB)];

  // Label is as wide as the content area of the view.
  [constraints addObject:[text.widthAnchor
                             constraintEqualToAnchor:self.layoutMarginsGuide
                                                         .widthAnchor]];

  // Parse out link location from the label text.
  NSString* textString =
      l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_SHORT_DESCRIPTION);
  NSRange linkRange;
  textString = ParseStringWithLink(textString, &linkRange);

  // Build style attributes for the label.
  NSMutableParagraphStyle* paragraphStyle =
      [[[NSMutableParagraphStyle alloc] init] autorelease];
  [paragraphStyle setLineBreakMode:NSLineBreakByWordWrapping];
  [paragraphStyle setLineHeightMultiple:kLineSpace];
  NSDictionary* attributes = @{
    NSParagraphStyleAttributeName : paragraphStyle,
    NSFontAttributeName :
        [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:kTextFontSize],
    NSForegroundColorAttributeName :
        [UIColor colorWithWhite:kTextColorGrayShade alpha:1]
  };

  // Create and assign attributed text to label.
  base::scoped_nsobject<NSMutableAttributedString> attributedText(
      [[NSMutableAttributedString alloc] initWithString:textString]);
  [attributedText setAttributes:attributes
                          range:NSMakeRange(0, textString.length)];
  text.attributedText = attributedText;
  [_linkController addLinkWithRange:linkRange
                                url:GURL("contextualSearch://settings")];
  text.numberOfLines = 0;

  UIFont* buttonFont =
      [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:kButtonFontSize];

  // Create accept and decline buttons with dimensions defined by the
  // minimum height and width constants.
  MDCFlatButton* acceptButton = [[[MDCFlatButton alloc] init] autorelease];
  acceptButton.hasOpaqueBackground = YES;
  acceptButton.inkColor =
      [[[MDCPalette cr_bluePalette] tint300] colorWithAlphaComponent:0.5f];
  [acceptButton setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]
                          forState:UIControlStateNormal];
  [acceptButton setBackgroundColor:[UIColor colorWithWhite:0.6f alpha:1.0f]
                          forState:UIControlStateDisabled];
  [constraints addObjectsFromArray:@[
    [acceptButton.widthAnchor
        constraintGreaterThanOrEqualToConstant:kButtonMinWidth],
    [acceptButton.heightAnchor constraintEqualToConstant:kButtonHeight]
  ]];
  [acceptButton
      setTitle:l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_GOT_IT_BUTTON)
      forState:UIControlStateNormal];
  acceptButton.customTitleColor = [UIColor whiteColor];
  [acceptButton addTarget:_delegate
                   action:@selector(promoViewAcceptTapped)
         forControlEvents:UIControlEventTouchUpInside];
  [acceptButton.titleLabel setFont:buttonFont];

  UIColor* customTitleColor = [[MDCPalette cr_bluePalette] tint500];
  MDCButton* declineButton = [[[MDCFlatButton alloc] init] autorelease];
  [constraints addObjectsFromArray:@[
    [declineButton.widthAnchor
        constraintGreaterThanOrEqualToConstant:kButtonMinWidth],
    [declineButton.heightAnchor constraintEqualToConstant:kButtonHeight]
  ]];
  [declineButton setTitleColor:customTitleColor forState:UIControlStateNormal];
  [declineButton
      setTitle:l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_PROMO_OPTOUT)
      forState:UIControlStateNormal];
  [declineButton addTarget:_delegate
                    action:@selector(promoViewDeclineTapped)
          forControlEvents:UIControlEventTouchUpInside];
  [declineButton.titleLabel setFont:buttonFont];

  // Create the divider (a simple colored view) with height defined by
  // |kDividerHeight| and width spanning this view's width.
  UIView* divider = [[[UIView alloc] initWithFrame:CGRectZero] autorelease];
  divider.backgroundColor = [UIColor colorWithWhite:0.745 alpha:1.0];
  [constraints addObjectsFromArray:@[
    [divider.widthAnchor constraintEqualToAnchor:self.widthAnchor],
    [divider.heightAnchor constraintEqualToConstant:kDividerHeight],
  ]];

  self.translatesAutoresizingMaskIntoConstraints = NO;
  text.translatesAutoresizingMaskIntoConstraints = NO;
  acceptButton.translatesAutoresizingMaskIntoConstraints = NO;
  declineButton.translatesAutoresizingMaskIntoConstraints = NO;
  divider.translatesAutoresizingMaskIntoConstraints = NO;

  [self addSubview:text];
  [self addSubview:acceptButton];
  [self addSubview:declineButton];
  [self addSubview:divider];

  self.layoutMargins = UIEdgeInsetsMake(kMargin, kMargin, kMargin, kMargin);

  // Position all of the subviews
  [constraints addObjectsFromArray:@[
    // Text is at the top of the content area, centered horizontally.
    [text.topAnchor constraintEqualToAnchor:self.layoutMarginsGuide.topAnchor],
    [text.centerXAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.centerXAnchor],
    // Accept button is |kSpaceBelowText| below the text, and at the bottom of
    // the content area.
    [acceptButton.topAnchor constraintEqualToAnchor:text.bottomAnchor
                                           constant:kSpaceBelowText],
    [acceptButton.bottomAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.bottomAnchor],
    // Decline button's top is aligned with the accept button's
    [declineButton.topAnchor constraintEqualToAnchor:acceptButton.topAnchor],
    // Decline button is to the leading side of the accept button, which
    // abuts the trailing edge of the content area.
    [acceptButton.leadingAnchor
        constraintEqualToAnchor:declineButton.trailingAnchor
                       constant:kButtonSeparator],
    [acceptButton.trailingAnchor
        constraintEqualToAnchor:self.layoutMarginsGuide.trailingAnchor],
    // Divider is centered horizontally at the bottom of the view.
    [divider.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [divider.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:constraints];
  _constraints.reset([constraints retain]);

  return self;
}

- (void)dealloc {
  // Fix for crbug.com/591043 -- Some constraints applying to autoreleased
  // views may remain active with pointers to the to-be-deallocated views
  // after the views are no longer being displayed. This ensures that all
  // constraints are disabled and will not be applied even if some subviews
  // are lingering.
  [NSLayoutConstraint deactivateConstraints:_constraints];
  [super dealloc];
}

- (void)setHidden:(BOOL)hidden {
  if (!hidden && self.disabled)
    return;
  [self setClosure:hidden ? 0.0 : 1.0];
  [super setHidden:hidden];
}

- (void)setClosure:(CGFloat)closure {
  self.transform = CGAffineTransformMakeScale(1.0, closure);
}

- (void)closeAnimated:(BOOL)animated {
  [UIView cr_animateWithDuration:animated ? kCloseDuration : 0
                           delay:0
                           curve:ios::material::CurveEaseOut
                         options:UIViewAnimationOptionBeginFromCurrentState
                      animations:^{
                        self.hidden = YES;
                      }
                      completion:nil];
}

#pragma mark - ContextualSearchPanelMotionObserver

- (void)panel:(ContextualSearchPanelView*)panel
    didChangeToState:(ContextualSearch::PanelState)toState
           fromState:(ContextualSearch::PanelState)fromState {
  if (self.disabled)
    return;
  if (toState == ContextualSearch::COVERING) {
    [self closeAnimated:YES];
  } else if (fromState == ContextualSearch::COVERING) {
    self.hidden = NO;
  }
}

- (void)panelWillPromote:(ContextualSearchPanelView*)panel {
  _delegate = nil;
  [panel removeMotionObserver:self];
}

@end
