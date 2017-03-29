// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill_edit_accessory_view.h"

#include <string>

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/image_util.h"
#include "ios/chrome/browser/ui/ui_util.h"

namespace {

const CGFloat kDefaultAccessoryHeight = 44;
const CGRect kDefaultAccessoryFrame = {{0, 0}, {0, kDefaultAccessoryHeight}};

const CGFloat kDefaultAccessoryButtonWidth = kDefaultAccessoryHeight;
const CGRect kDefaultAccessoryButtonRect = {
    {0, 0},
    {kDefaultAccessoryButtonWidth, kDefaultAccessoryHeight}};

const CGFloat kDefaultAccessorySeparatorWidth = 1;
const CGFloat kDefaultAccessorySeparatorHeight = kDefaultAccessoryHeight;
const CGRect kDefaultAccessorySeparatorRect = {
    {0, 0},
    {kDefaultAccessorySeparatorWidth, kDefaultAccessorySeparatorHeight}};

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

UIImageView* ImageViewWithImageName(NSString* imageName) {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);

  base::scoped_nsobject<UIImageView> imageView(
      [[UIImageView alloc] initWithImage:image]);
  [imageView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [imageView setFrame:kDefaultAccessorySeparatorRect];

  return imageView.autorelease();
}

}  // namespace

@interface AutofillEditAccessoryView () {
  base::scoped_nsobject<UIButton> _previousButton;
  base::scoped_nsobject<UIButton> _nextButton;
  __unsafe_unretained id<AutofillEditAccessoryDelegate> _delegate;  // weak
}
@end

@implementation AutofillEditAccessoryView

- (UIButton*)previousButton {
  return _previousButton;
}

- (UIButton*)nextButton {
  return _nextButton;
}

- (instancetype)initWithDelegate:(id<AutofillEditAccessoryDelegate>)delegate {
  self = [super initWithFrame:kDefaultAccessoryFrame];
  if (!self)
    return nil;

  _delegate = delegate;
  [self addBackgroundImage];
  [self setupSubviews];

  return self;
}

- (void)setupSubviews {
  UIButton* previousButton =
      [self accessoryButtonWithNormalName:@"autofill_prev"
                              pressedName:@"autofill_prev_pressed"
                             disabledName:@"autofill_prev_inactive"
                                   action:@selector(previousPressed)];
  [self addSubview:previousButton];
  _previousButton.reset([previousButton retain]);

  UIImageView* firstSeparator = ImageViewWithImageName(@"autofill_middle_sep");
  [self addSubview:firstSeparator];

  UIButton* nextButton =
      [self accessoryButtonWithNormalName:@"autofill_next"
                              pressedName:@"autofill_next_pressed"
                             disabledName:@"autofill_next_inactive"
                                   action:@selector(nextPressed)];
  [self addSubview:nextButton];
  _nextButton.reset([nextButton retain]);

  UIImageView* secondSeparator = nil;
  UIButton* closeButton = nil;
  if (!IsIPadIdiom()) {
    closeButton = [self accessoryButtonWithNormalName:@"autofill_close"
                                          pressedName:@"autofill_close_pressed"
                                         disabledName:nil
                                               action:@selector(closePressed)];

    [self addSubview:closeButton];

    secondSeparator = ImageViewWithImageName(@"autofill_middle_sep");
    [self addSubview:secondSeparator];
  }

  NSDictionary* bindings = NSDictionaryOfVariableBindings(
      previousButton, firstSeparator, nextButton);
  NSString* horizontalLayout =
      @"H:[previousButton][firstSeparator][nextButton]|";
  if (closeButton) {
    bindings = NSDictionaryOfVariableBindings(previousButton, firstSeparator,
                                              nextButton, secondSeparator,
                                              closeButton);
    horizontalLayout = @"H:[previousButton][firstSeparator][nextButton]["
                       @"secondSeparator][closeButton]|";
  }

  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:horizontalLayout
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[previousButton]|"
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[firstSeparator]|"
                                               options:0
                                               metrics:0
                                                 views:bindings]];
  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"V:[nextButton]|"
                                               options:0
                                               metrics:nil
                                                 views:bindings]];

  if (closeButton) {
    [self addConstraints:[NSLayoutConstraint
                             constraintsWithVisualFormat:@"V:[secondSeparator]|"
                                                 options:0
                                                 metrics:nil
                                                   views:bindings]];
    [self addConstraints:[NSLayoutConstraint
                             constraintsWithVisualFormat:@"V:[closeButton]|"
                                                 options:0
                                                 metrics:nil
                                                   views:bindings]];
  }
}

- (UIButton*)accessoryButtonWithNormalName:(NSString*)normalName
                               pressedName:(NSString*)pressedName
                              disabledName:(NSString*)disabledName
                                    action:(SEL)action {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];

  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button setFrame:kDefaultAccessoryButtonRect];

  [button setBackgroundImage:ButtonImage(normalName)
                    forState:UIControlStateNormal];
  [button setBackgroundImage:ButtonImage(pressedName)
                    forState:UIControlStateSelected];
  [button setBackgroundImage:ButtonImage(pressedName)
                    forState:UIControlStateHighlighted];

  if (disabledName) {
    [button setBackgroundImage:ButtonImage(disabledName)
                      forState:UIControlStateDisabled];
  }

  [button addTarget:_delegate
                action:action
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

- (void)addBackgroundImage {
  UIImage* backgroundImage =
      StretchableImageNamed(@"autofill_keyboard_background");

  UIImageView* backgroundImageView =
      [[UIImageView alloc] initWithFrame:[self bounds]];
  [backgroundImageView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [backgroundImageView setImage:backgroundImage];
  [self insertSubview:backgroundImageView atIndex:0];
  [backgroundImageView release];
}

@end
