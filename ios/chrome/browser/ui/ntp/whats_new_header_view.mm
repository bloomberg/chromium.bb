// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/whats_new_header_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/common/string_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kLabelTopMargin = 16;
const CGFloat kLabelBottomMargin = 24;
const CGFloat kLabelLineSpacing = 4;
const CGFloat kLabelLeftMargin = 8;
const CGFloat kLabelFontSize = 14;
const CGFloat kInfoIconSize = 24;
const CGFloat kInfoIconTopMargin = 12;

const int kTextColorRgb = 0x333333;
const int kLinkColorRgb = 0x5595FE;

}  // namespace

@interface WhatsNewHeaderView () {
  UIImageView* _infoIconImageView;
  UILabel* _promoLabel;
  NSLayoutConstraint* _edgeConstraint;
  UIView* _leftSpacer;
  UIView* _rightSpacer;
  CGFloat _sideMargin;
}

@end

@implementation WhatsNewHeaderView

@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.hidden = YES;
    UIImage* infoIconImage = ios::GetChromeBrowserProvider()
                                 ->GetBrandedImageProvider()
                                 ->GetWhatsNewIconImage(WHATS_NEW_INFO);
    _infoIconImageView = [[UIImageView alloc] initWithImage:infoIconImage];
    _promoLabel = [[self class] promoLabel];
    [_promoLabel setUserInteractionEnabled:YES];
    UITapGestureRecognizer* promoTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(promoButtonPressed)];
    [_promoLabel addGestureRecognizer:promoTapRecognizer];
    _leftSpacer = [[UIView alloc] initWithFrame:CGRectZero];
    _rightSpacer = [[UIView alloc] initWithFrame:CGRectZero];

    [_leftSpacer setHidden:YES];
    [_rightSpacer setHidden:YES];

    [self addSubview:_infoIconImageView];
    [self addSubview:_promoLabel];
    [self addSubview:_leftSpacer];
    [self addSubview:_rightSpacer];
    [self setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_infoIconImageView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_promoLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_leftSpacer setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_rightSpacer setTranslatesAutoresizingMaskIntoConstraints:NO];
  }
  return self;
}

- (void)updateConstraints {
  if ([[self constraints] count] == 0 && !self.hidden) {
    NSString* verticalLogoVisualFormat =
        @"V:|-logoTopMargin-[iconView(==iconSize)]";
    NSString* verticalLabelVisualFormat = @"V:|-topMargin-[promoLabel]";
    NSString* horizontalVisualFormat =
        @"H:|-0-[leftSpacer]-0-[iconView(==iconSize)]-"
        @"iconTextSpace-[promoLabel]-0-[rightSpacer(==leftSpacer)]-0-|";
    NSNumber* topMargin = @(kLabelTopMargin);
    NSNumber* iconSize = @(kInfoIconSize);
    NSNumber* iconTextSpace = @(kLabelLeftMargin);
    NSNumber* logoTopMargin = @(kInfoIconTopMargin);
    NSDictionary* metrics = NSDictionaryOfVariableBindings(
        topMargin, iconSize, iconTextSpace, logoTopMargin);

    NSDictionary* views = @{
      @"promoLabel" : _promoLabel,
      @"iconView" : _infoIconImageView,
      @"leftSpacer" : _leftSpacer,
      @"rightSpacer" : _rightSpacer,
    };

    ApplyVisualConstraintsWithMetrics(
        @[
          verticalLogoVisualFormat, verticalLabelVisualFormat,
          horizontalVisualFormat
        ],
        views, metrics, self);

    _edgeConstraint = [NSLayoutConstraint
        constraintWithItem:_leftSpacer
                 attribute:NSLayoutAttributeWidth
                 relatedBy:NSLayoutRelationGreaterThanOrEqual
                    toItem:nil
                 attribute:NSLayoutAttributeNotAnAttribute
                multiplier:1
                  constant:_sideMargin];
    [self addConstraint:_edgeConstraint];
  }
  [_edgeConstraint setConstant:_sideMargin];
  [super updateConstraints];
}

- (void)setText:(NSString*)text {
  [[self class] setText:text inPromoLabel:_promoLabel];
  self.hidden = NO;
}

- (void)setIcon:(WhatsNewIcon)icon {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetBrandedImageProvider()
                       ->GetWhatsNewIconImage(icon);
  [_infoIconImageView setImage:image];
}

- (void)setSideMargin:(CGFloat)sideMargin forWidth:(CGFloat)width {
  _sideMargin = sideMargin;
  [self setNeedsUpdateConstraints];
  CGFloat maxLabelWidth =
      width - 2 * sideMargin - kInfoIconSize - kLabelLeftMargin;
  [_promoLabel setPreferredMaxLayoutWidth:maxLabelWidth];
}

- (void)promoButtonPressed {
  [_delegate onPromoLabelTapped];
  [self removeConstraints:self.constraints];
  _edgeConstraint = nil;
}

+ (void)setText:(NSString*)promoLabelText inPromoLabel:(UILabel*)promoLabel {
  DCHECK(promoLabelText);
  NSRange linkRange;
  NSString* strippedText = ParseStringWithLink(promoLabelText, &linkRange);
  DCHECK_NE(NSNotFound, static_cast<NSInteger>(linkRange.location));
  DCHECK_NE(0u, linkRange.length);

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:strippedText];

  // Sets the styling to mimic a link.
  UIColor* linkColor = UIColorFromRGB(kLinkColorRgb, 1.0);
  [attributedText addAttribute:NSForegroundColorAttributeName
                         value:linkColor
                         range:linkRange];
  [attributedText addAttribute:NSUnderlineStyleAttributeName
                         value:@(NSUnderlineStyleSingle)
                         range:linkRange];
  [attributedText addAttribute:NSUnderlineColorAttributeName
                         value:linkColor
                         range:linkRange];

  // Sets the line spacing on the attributed string.
  NSInteger strLength = [strippedText length];
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kLabelLineSpacing];
  [attributedText addAttribute:NSParagraphStyleAttributeName
                         value:style
                         range:NSMakeRange(0, strLength)];

  [promoLabel setAttributedText:attributedText];
}

+ (UILabel*)promoLabel {
  UILabel* promoLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [promoLabel
      setFont:[[MDCTypography fontLoader] regularFontOfSize:kLabelFontSize]];
  [promoLabel setTextColor:UIColorFromRGB(kTextColorRgb, 1.0)];
  [promoLabel setNumberOfLines:0];
  [promoLabel setTextAlignment:NSTextAlignmentNatural];
  [promoLabel setLineBreakMode:NSLineBreakByWordWrapping];
  return promoLabel;
}

+ (int)heightToFitText:(NSString*)text inWidth:(CGFloat)width {
  CGFloat maxWidthForLabel = width - kInfoIconSize - kLabelLeftMargin;
  UILabel* promoLabel = [self promoLabel];
  [[self class] setText:text inPromoLabel:promoLabel];
  CGFloat promoLabelHeight =
      [promoLabel sizeThatFits:CGSizeMake(maxWidthForLabel, CGFLOAT_MAX)]
          .height;
  return promoLabelHeight + kLabelTopMargin + kLabelBottomMargin;
}

@end
