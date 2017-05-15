// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_button.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_bar_item.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

namespace {

const int kButtonColor = 0x333333;
const int kButtonSelectedColor = 0x4285F4;

}  // anonymous namespace

@interface NewTabPageBarButton () {
  UIColor* _color;
  UIColor* _selectedColor;
  UIColor* _incognitoColor;
  UIColor* _incognitoSelectedColor;
  UIColor* _interpolatedColor;
  UIColor* _interpolatedSelectedColor;

  UIImage* _image;
  NSString* _title;
  base::mac::ObjCPropertyReleaser _propertyReleaser_NewTabPageBarButton;
}

@property(nonatomic, retain) UIColor* color;
@property(nonatomic, retain) UIColor* selectedColor;
@property(nonatomic, retain) UIColor* incognitoColor;
@property(nonatomic, retain) UIColor* incognitoSelectedColor;
@property(nonatomic, retain) UIColor* interpolatedColor;
@property(nonatomic, retain) UIColor* interpolatedSelectedColor;
@property(nonatomic, retain) UIImage* image;
@property(nonatomic, copy) NSString* title;

// Sets the tint color of the button to |interpolatedColor| or
// |interpolatedSelectedColor|, depending on the state of the button.
- (void)refreshTintColor;

@end

@implementation NewTabPageBarButton

@synthesize color = _color;
@synthesize selectedColor = _selectedColor;
@synthesize incognitoColor = _incognitoColor;
@synthesize incognitoSelectedColor = _incognitoSelectedColor;
@synthesize interpolatedColor = _interpolatedColor;
@synthesize interpolatedSelectedColor = _interpolatedSelectedColor;
@synthesize image = _image;
@synthesize title = _title;

+ (instancetype)buttonWithItem:(NewTabPageBarItem*)item {
  DCHECK(item);
  DCHECK(item.title);
  DCHECK(item.image);
  NewTabPageBarButton* button =
      [[self class] buttonWithType:UIButtonTypeCustom];
  button->_propertyReleaser_NewTabPageBarButton.Init(
      button, [NewTabPageBarButton class]);

  button.title = item.title;
  button.image =
      [item.image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  button.color = UIColorFromRGB(kButtonColor, 1.0);
  button.selectedColor = UIColorFromRGB(kButtonSelectedColor, 1.0);
  button.incognitoColor = [UIColor colorWithWhite:1 alpha:0.5];
  button.incognitoSelectedColor = [UIColor whiteColor];

  button.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  button.adjustsImageWhenHighlighted = NO;
  button.accessibilityLabel = item.title;
  button.titleLabel.font = [MDCTypography body2Font];
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6;

  [button useIncognitoColorScheme:0];
  [button setContentToDisplay:new_tab_page_bar_button::ContentType::TEXT];
  return button;
}

- (void)useIncognitoColorScheme:(CGFloat)percentage {
  DCHECK(percentage >= 0 && percentage <= 1);
  self.interpolatedColor =
      InterpolateFromColorToColor(_color, _incognitoColor, percentage);
  self.interpolatedSelectedColor = InterpolateFromColorToColor(
      _selectedColor, _incognitoSelectedColor, percentage);

  [self setTitleColor:_interpolatedColor forState:UIControlStateNormal];
  [self setTitleColor:_interpolatedSelectedColor
             forState:UIControlStateSelected];
  [self setTitleColor:_interpolatedSelectedColor
             forState:UIControlStateHighlighted];

  [self refreshTintColor];
}

- (void)setContentToDisplay:(new_tab_page_bar_button::ContentType)contentType {
  switch (contentType) {
    case new_tab_page_bar_button::ContentType::IMAGE:
      [self setImage:_image forState:UIControlStateNormal];
      [self setTitle:nil forState:UIControlStateNormal];
      break;
    case new_tab_page_bar_button::ContentType::TEXT:
      [self setImage:nil forState:UIControlStateNormal];
      [self setTitle:_title forState:UIControlStateNormal];
      break;
  }
}

- (void)refreshTintColor {
  if (self.selected || self.highlighted) {
    [self setTintColor:_interpolatedSelectedColor];
  } else {
    [self setTintColor:_interpolatedColor];
  }
}

#pragma mark -
#pragma mark UIButton overrides

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  [self refreshTintColor];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self refreshTintColor];
}

@end
