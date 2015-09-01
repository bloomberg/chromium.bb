// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_label.h"

#include <cmath>

#import <QuartzCore/QuartzCore.h>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"
#include "ios/chrome/browser/ui/ui_util.h"

namespace {

// The button corner radius.
const CGFloat kCornerRadius = 3.0f;

// The width of the border in the button background image.
const CGFloat kBorderWidth = 1.0f;

// Font size of button titles.
const CGFloat kIpadFontSize = 15.0f;
const CGFloat kIphoneFontSize = 13.0f;

// The grayscale value of the color object.
const CGFloat kTitleColor = 51.0f / 255.0f;

// The alpha value of the suggestion's description label.
const CGFloat kDescriptionLabelAlpha = 0.54f;

// The edge inset for background image.
const CGFloat kBackgroundImageEdgeInsetSize = 8.0f;
// The space between items in the label.
const CGFloat kSpacing = 8.0f;

// Structure that record the image for each icon.
struct IconImageMap {
  const char* const icon_name;
  NSString* image_name;
};

const IconImageMap kCreditCardIconImageMap[] = {
    {autofill::kAmericanExpressCard, @"autofill_card_american_express"},
    {autofill::kDiscoverCard, @"autofill_card_discover"},
    {autofill::kMasterCard, @"autofill_card_mastercard"},
    {autofill::kVisaCard, @"autofill_card_visa"},
    {autofill::kDinersCard, @"autofill_card_diners"},
    {autofill::kGenericCard, @"autofill_card_generic"},
    {autofill::kJCBCard, @"autofill_card_jcb"},
    {autofill::kUnionPay, @"autofill_card_unionpay"},
};

// Creates a label with the given |text| and |alpha| suitable for use in a
// suggestion button in the keyboard accessory view.
UILabel* TextLabel(NSString* text, CGFloat alpha) {
  base::scoped_nsobject<UILabel> label([[UILabel alloc] init]);
  [label setText:text];
  UIFont* font = IsIPadIdiom() ? [UIFont systemFontOfSize:kIpadFontSize]
                               : [UIFont systemFontOfSize:kIphoneFontSize];
  [label setFont:font];
  [label setTextColor:[UIColor colorWithWhite:kTitleColor alpha:alpha]];
  [label setBackgroundColor:[UIColor clearColor]];
  [label sizeToFit];
  return label.autorelease();
}

}  // namespace

@interface FormSuggestionLabel ()

@property(nonatomic, readonly) UIColor* normalBackgroundColor;
@property(nonatomic, readonly) UIColor* pressedBackgroundColor;

// Returns the color generated from the image named |imageName| resized to
// |rect|.
- (UIColor*)backgroundColorFromImageNamed:(NSString*)imageName
                                   inRect:(CGRect)rect;
// Returns the name of the image for credit card icon.
+ (NSString*)imageNameForCreditCardIcon:(NSString*)icon;
@end

@implementation FormSuggestionLabel {
  // Client of this view.
  base::WeakNSProtocol<id<FormSuggestionViewClient>> client_;
  base::scoped_nsobject<FormSuggestion> suggestion_;

  // Background color when the label is not pressed.
  base::scoped_nsobject<UIColor> normalBackgroundColor_;
  // Background color when the label is pressed.
  base::scoped_nsobject<UIColor> pressedBackgroundColor_;
}

- (id)initWithSuggestion:(FormSuggestion*)suggestion
           proposedFrame:(CGRect)proposedFrame
                  client:(id<FormSuggestionViewClient>)client {
  // TODO(jimblackler): implement sizeThatFits: and layoutSubviews, and perform
  // layout in those methods instead of in the designated initializer.
  self = [super initWithFrame:CGRectZero];
  if (self) {
    suggestion_.reset([suggestion retain]);
    client_.reset(client);

    const CGFloat frameHeight = CGRectGetHeight(proposedFrame);
    CGFloat currentX = kBorderWidth + kSpacing;

    // [UIImage imageNamed:] writes error message if nil is passed. Prevent
    // console spam by checking the name first.
    NSString* iconImageName =
        [FormSuggestionLabel imageNameForCreditCardIcon:suggestion.icon];
    UIImage* iconImage = nil;
    if (iconImageName)
      iconImage = [UIImage imageNamed:iconImageName];
    if (iconImage) {
      UIImageView* iconView =
          [[[UIImageView alloc] initWithImage:iconImage] autorelease];
      const CGFloat iconY =
          std::floor((frameHeight - iconImage.size.height) / 2.0f);
      iconView.frame = CGRectMake(currentX, iconY, iconImage.size.width,
                                  iconImage.size.height);
      [self addSubview:iconView];
      currentX += CGRectGetWidth(iconView.frame) + kSpacing;
    }

    UILabel* label = TextLabel(suggestion.value, 1.0f);
    const CGFloat labelY =
        std::floor(frameHeight / 2.0f - CGRectGetMidY(label.frame));
    label.frame = CGRectMake(currentX, labelY, CGRectGetWidth(label.frame),
                             CGRectGetHeight(label.frame));
    [self addSubview:label];
    currentX += CGRectGetWidth(label.frame) + kSpacing;

    if (suggestion.displayDescription) {
      UILabel* description =
          TextLabel(suggestion.displayDescription, kDescriptionLabelAlpha);
      const CGFloat descriptionY =
          std::floor(frameHeight / 2.0f - CGRectGetMidY(description.frame));
      description.frame =
          CGRectMake(currentX, descriptionY, CGRectGetWidth(description.frame),
                     CGRectGetHeight(description.frame));
      [self addSubview:description];
      currentX += CGRectGetWidth(description.frame) + kSpacing;
    }

    currentX += kBorderWidth;

    self.frame = CGRectMake(proposedFrame.origin.x, proposedFrame.origin.y,
                            currentX, proposedFrame.size.height);
    [self setBackgroundColor:[self normalBackgroundColor]];
    [[self layer] setCornerRadius:kCornerRadius];

    [self setClipsToBounds:YES];
    [self setIsAccessibilityElement:YES];
    [self setAccessibilityLabel:suggestion.value];
    [self setUserInteractionEnabled:YES];
  }

  return self;
}

- (id)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)layoutSubviews {
  // Resets the colors so the size will be updated in their getters.
  normalBackgroundColor_.reset();
  pressedBackgroundColor_.reset();
  [super layoutSubviews];
}

#pragma mark -
#pragma mark UIResponder

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:self.pressedBackgroundColor];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:self.normalBackgroundColor];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:self.normalBackgroundColor];
  [client_ didSelectSuggestion:suggestion_];
}

#pragma mark -
#pragma mark Private

- (UIColor*)normalBackgroundColor {
  if (!normalBackgroundColor_) {
    normalBackgroundColor_.reset(
        [[self backgroundColorFromImageNamed:@"autofill_button"
                                      inRect:self.bounds] retain]);
  }
  return normalBackgroundColor_;
}

- (UIColor*)pressedBackgroundColor {
  if (!pressedBackgroundColor_) {
    pressedBackgroundColor_.reset(
        [[self backgroundColorFromImageNamed:@"autofill_button_pressed"
                                      inRect:self.bounds] retain]);
  }
  return pressedBackgroundColor_;
}

- (UIColor*)backgroundColorFromImageNamed:(NSString*)imageName
                                   inRect:(CGRect)rect {
  UIEdgeInsets edgeInsets = UIEdgeInsetsMake(
      kBackgroundImageEdgeInsetSize, kBackgroundImageEdgeInsetSize,
      kBackgroundImageEdgeInsetSize, kBackgroundImageEdgeInsetSize);
  UIImage* image =
      [[UIImage imageNamed:imageName] resizableImageWithCapInsets:edgeInsets];

  UIGraphicsBeginImageContextWithOptions(rect.size, NO,
                                         UIScreen.mainScreen.scale);
  [image drawInRect:rect];
  UIImage* resizedImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  return [UIColor colorWithPatternImage:resizedImage];
}

+ (NSString*)imageNameForCreditCardIcon:(NSString*)icon {
  if (!icon || [icon length] == 0) {
    return nil;
  }
  std::string iconName(base::SysNSStringToUTF8(icon));
  for (size_t i = 0; i < arraysize(kCreditCardIconImageMap); ++i) {
    if (iconName.compare(kCreditCardIconImageMap[i].icon_name) == 0) {
      return kCreditCardIconImageMap[i].image_name;
    }
  }
  return nil;
}

@end
