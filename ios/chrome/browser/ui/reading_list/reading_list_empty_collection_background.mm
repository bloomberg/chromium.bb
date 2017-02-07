// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_empty_collection_background.h"

#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Images name.
NSString* const kEmptyReadingListBackgroundIcon = @"reading_list_empty_state";
NSString* const kToolbarMenuIcon = @"reading_list_toolbar_icon";
NSString* const kShareMenuIcon = @"reading_list_share_icon";

// Tag in string.
NSString* const kOpenShareMarker = @"SHARE_OPENING_ICON";

// Background view constants.
const CGFloat kTextImageSpacing = 10;
const CGFloat kTextHorizontalMargin = 56;
const CGFloat kImageWidth = 60;
const CGFloat kImageHeight = 44;
const CGFloat kFontSize = 16;
const CGFloat kIconSize = 24;
const CGFloat kLineHeight = 24;
const CGFloat kPercentageFromTopForPosition = 0.4;
const CGFloat kAlphaForCaret = 0.7;

}  // namespace

@interface ReadingListEmptyCollectionBackground ()

// Attaches the icon named |iconName| to |instructionString| and a |caret|. The
// icon is positionned using the |iconOffset| and with the |attributes| (mainly
// the color).
- (void)attachIconNamed:(NSString*)iconName
               toString:(NSMutableAttributedString*)instructionString
              withCaret:(NSMutableAttributedString*)caret
                 offset:(CGFloat)iconOffset
        imageAttributes:(NSDictionary*)attributes;
// Sets the constraints for this view, positionning the |imageView| in the X
// center and at 40% of the top. The |label| is positionned below.
- (void)setConstraintsToImageView:(UIImageView*)imageView
                         andLabel:(UILabel*)label;
// Returns the caret string to be displayed.
- (NSString*)caretString;

@end

@implementation ReadingListEmptyCollectionBackground

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    NSString* rawText =
        l10n_util::GetNSString(IDS_IOS_READING_LIST_EMPTY_MESSAGE);
    // Add a space between Read Later and the preceding caret.
    NSString* readLater = [@" "
        stringByAppendingString:l10n_util::GetNSString(
                                    IDS_IOS_SHARE_MENU_READING_LIST_ACTION)];

    id<MDCTypographyFontLoading> fontLoader = [MDCTypography fontLoader];

    UIColor* textColor = [[MDCPalette greyPalette] tint700];
    UIFont* textFont = [fontLoader regularFontOfSize:kFontSize];
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineSpacing = kLineHeight - textFont.lineHeight;

    NSDictionary* attributes = @{
      NSFontAttributeName : textFont,
      NSForegroundColorAttributeName : textColor,
      NSParagraphStyleAttributeName : paragraphStyle
    };

    // Offset to vertically center the icons.
    CGFloat iconOffset = (textFont.xHeight - kIconSize) / 2.0;

    UIColor* caretColor = [textColor colorWithAlphaComponent:kAlphaForCaret];
    UIFont* instructionFont = [fontLoader mediumFontOfSize:kFontSize];
    NSDictionary* caretAttributes = @{
      NSFontAttributeName : instructionFont,
      NSForegroundColorAttributeName : caretColor,
    };

    NSDictionary* instructionAttributes = @{
      NSFontAttributeName : instructionFont,
      NSForegroundColorAttributeName : textColor,
    };

    NSMutableAttributedString* baseAttributedString =
        [[NSMutableAttributedString alloc] initWithString:rawText
                                               attributes:attributes];

    NSMutableAttributedString* caret =
        [[NSMutableAttributedString alloc] initWithString:[self caretString]
                                               attributes:caretAttributes];

    NSMutableAttributedString* instructionString =
        [[NSMutableAttributedString alloc] init];

    // Add the images inside the string.
    if (IsCompact()) {
      // If the device has a compact display the share menu is accessed from the
      // toolbar menu. If it is expanded, the share menu is directly accessible.
      [self attachIconNamed:kToolbarMenuIcon
                   toString:instructionString
                  withCaret:caret
                     offset:iconOffset
            imageAttributes:attributes];
    }

    [self attachIconNamed:kShareMenuIcon
                 toString:instructionString
                withCaret:caret
                   offset:iconOffset
          imageAttributes:attributes];

    // Add the "Read Later" string.
    NSAttributedString* shareMenuAction =
        [[NSAttributedString alloc] initWithString:readLater
                                        attributes:instructionAttributes];
    [instructionString appendAttributedString:shareMenuAction];

    NSRange iconRange =
        [[baseAttributedString string] rangeOfString:kOpenShareMarker];
    [baseAttributedString replaceCharactersInRange:iconRange
                              withAttributedString:instructionString];

    // Attach to the label.
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.attributedText = baseAttributedString;
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    [label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:label];

    UIImageView* imageView = [[UIImageView alloc] init];
    imageView.image = [UIImage imageNamed:kEmptyReadingListBackgroundIcon];
    [imageView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:imageView];

    [self setConstraintsToImageView:imageView andLabel:label];
  }
  return self;
}

- (void)attachIconNamed:(NSString*)iconName
               toString:(NSMutableAttributedString*)instructionString
              withCaret:(NSMutableAttributedString*)caret
                 offset:(CGFloat)iconOffset
        imageAttributes:(NSDictionary*)attributes {
  // Add a zero width space to set the attributes for the image.
  [instructionString appendAttributedString:[[NSAttributedString alloc]
                                                initWithString:@"\u200B"
                                                    attributes:attributes]];

  NSTextAttachment* toolbarIcon = [[NSTextAttachment alloc] init];
  toolbarIcon.image = [[UIImage imageNamed:iconName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  toolbarIcon.bounds = CGRectMake(0, iconOffset, kIconSize, kIconSize);
  [instructionString
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:toolbarIcon]];
  [instructionString appendAttributedString:caret];
}

- (void)setConstraintsToImageView:(UIImageView*)imageView
                         andLabel:(UILabel*)label {
  [NSLayoutConstraint activateConstraints:@[
    [[imageView heightAnchor] constraintEqualToConstant:kImageHeight],
    [[imageView widthAnchor] constraintEqualToConstant:kImageWidth],
    [[self centerXAnchor] constraintEqualToAnchor:label.centerXAnchor],
    [[self centerXAnchor] constraintEqualToAnchor:imageView.centerXAnchor],
    [label.topAnchor constraintEqualToAnchor:imageView.bottomAnchor
                                    constant:kTextImageSpacing]
  ]];

  // Position the top of the image at 40% from the top.
  NSLayoutConstraint* verticalAlignment =
      [NSLayoutConstraint constraintWithItem:imageView
                                   attribute:NSLayoutAttributeTop
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:self
                                   attribute:NSLayoutAttributeBottom
                                  multiplier:kPercentageFromTopForPosition
                                    constant:0];
  [self addConstraints:@[ verticalAlignment ]];

  ApplyVisualConstraintsWithMetrics(@[ @"H:|-(margin)-[textLabel]-(margin)-|" ],
                                    @{ @"textLabel" : label },
                                    @{ @"margin" : @(kTextHorizontalMargin) });
}

- (NSString*)caretString {
  if (UseRTLLayout())
    return @"\u25C2";
  return @"\u25B8";
}

@end
