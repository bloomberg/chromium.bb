// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/chrome/browser/ui/image_util.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The alpha value of the background color.
const CGFloat kBackgroundColorAlpha = 1.0;

// Horizontal margin around the custom view.
const CGFloat kCustomViewHorizontalMargin = 2;

// The width of the separators of the previous and next buttons.
const CGFloat kNavigationButtonSeparatorWidth = 1;

// The width of the shadow part of the navigation area separator.
const CGFloat kNavigationAreaSeparatorShadowWidth = 2;

// The width of the navigation area / custom view separator asset.
const CGFloat kNavigationAreaSeparatorWidth = 1;

// Returns YES if the keyboard close button should be shown on the accessory.
BOOL ShouldShowCloseButton() {
  return !IsIPadIdiom();
}

}  // namespace

@interface FormInputAccessoryView ()

// Returns a view that shows navigation buttons.
- (UIView*)viewForNavigationButtons;

// Adds a navigation button for Autofill in |view| that has |normalImage| for
// state UIControlStateNormal, a |pressedImage| for states
// UIControlStateSelected and UIControlStateHighlighted, and an optional
// |disabledImage| for UIControlStateDisabled.
- (UIButton*)addKeyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                    pressedImage:(UIImage*)pressedImage
                                   disabledImage:(UIImage*)disabledImage
                                          target:(id)target
                                          action:(SEL)action
                                         enabled:(BOOL)enabled
                                          inView:(UIView*)view;

// Adds a background image to |view|. The supplied image is stretched to fit the
// space by stretching the content its horizontal and vertical centers.
+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName;

// Adds an image view in |view| with an image named |imageName|.
+ (UIView*)createImageViewWithImageName:(NSString*)imageName
                                 inView:(UIView*)view;

@end

@implementation FormInputAccessoryView {
  // Delegate of this view.
  __weak id<FormInputAccessoryViewDelegate> _delegate;
}

- (instancetype)initWithDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  DCHECK(delegate);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame customView:(UIView*)customView {
  self = [super initWithFrame:frame];
  if (self) {
    customView.frame =
        CGRectMake(0, 0, CGRectGetWidth(frame), CGRectGetHeight(frame));
    [self addSubview:customView];

    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];
  }
  return self;
}

- (void)initializeViewWithCustomView:(UIView*)customView
                           leftFrame:(CGRect)leftFrame
                          rightFrame:(CGRect)rightFrame {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* customViewContainer = [[UIView alloc] init];
  customViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:customViewContainer];
  UIView* navView = [[UIView alloc] init];
  navView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:navView];

  [customViewContainer addSubview:customView];
  customView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(customViewContainer, customView);

  UIView* navViewContent = [self viewForNavigationButtons];
  [navView addSubview:navViewContent];

  bool splitKeyboard = CGRectGetWidth(rightFrame) != 0;
  if (splitKeyboard) {
    NSString* navViewBackgroundImageName = nil;
    NSString* customViewContainerBackgroundImageName = nil;
    BOOL isRTL = base::i18n::IsRTL();
    if (isRTL) {
      navView.frame = leftFrame;
      navViewBackgroundImageName = @"autofill_keyboard_background_left";
      customViewContainer.frame = rightFrame;
      customViewContainerBackgroundImageName =
          @"autofill_keyboard_background_right";
    } else {
      customViewContainer.frame = leftFrame;
      customViewContainerBackgroundImageName =
          @"autofill_keyboard_background_left";
      navView.frame = rightFrame;
      navViewBackgroundImageName = @"autofill_keyboard_background_right";
    }

    [[self class]
        addBackgroundImageInView:customViewContainer
                   withImageName:customViewContainerBackgroundImageName];
    [[self class] addBackgroundImageInView:navView
                             withImageName:navViewBackgroundImageName];

    [NSLayoutConstraint activateConstraints:@[
      [navViewContent.topAnchor constraintEqualToAnchor:navView.topAnchor],
      [navViewContent.bottomAnchor
          constraintEqualToAnchor:navView.bottomAnchor],
      [navViewContent.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:navView.leadingAnchor],
      [navViewContent.trailingAnchor
          constraintEqualToAnchor:navView.trailingAnchor],

      [customView.topAnchor
          constraintEqualToAnchor:customViewContainer.topAnchor],
      [customView.bottomAnchor
          constraintEqualToAnchor:customViewContainer.bottomAnchor],
      [customView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:customViewContainer
                                                   .leadingAnchor],
      [customView.trailingAnchor
          constraintEqualToAnchor:customViewContainer.trailingAnchor
                         constant:kCustomViewHorizontalMargin],
    ]];

  } else {
    AddSameConstraints(navView, navViewContent);

    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];

    UILayoutGuide* layoutGuide = SafeAreaLayoutGuideForView(self);
    [NSLayoutConstraint activateConstraints:@[
      [customViewContainer.topAnchor
          constraintEqualToAnchor:layoutGuide.topAnchor],
      [customViewContainer.bottomAnchor
          constraintEqualToAnchor:layoutGuide.bottomAnchor],
      [customViewContainer.leadingAnchor
          constraintEqualToAnchor:layoutGuide.leadingAnchor],
      [customViewContainer.trailingAnchor
          constraintEqualToAnchor:navView.leadingAnchor
                         constant:kNavigationAreaSeparatorShadowWidth],
      [navView.trailingAnchor
          constraintEqualToAnchor:layoutGuide.trailingAnchor],
      [navView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor],
      [navView.bottomAnchor constraintEqualToAnchor:layoutGuide.bottomAnchor],
    ]];
  }
}

#pragma mark -
#pragma mark UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark -
#pragma mark Private Methods

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

- (UIView*)viewForNavigationButtons {
  UIView* navView = [[UIView alloc] init];
  navView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* separator =
      [[self class] createImageViewWithImageName:@"autofill_left_sep"
                                          inView:navView];

  UIButton* previousButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_prev")
                             pressedImage:ButtonImage(@"autofill_prev_pressed")
                            disabledImage:ButtonImage(@"autofill_prev_inactive")
                                   target:_delegate
                                   action:@selector
                                   (selectPreviousElementWithButtonPress)
                                  enabled:NO
                                   inView:navView];
  [previousButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)];

  // Add internal separator.
  UIView* internalSeparator =
      [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                          inView:navView];

  UIButton* nextButton = [self
      addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_next")
                             pressedImage:ButtonImage(@"autofill_next_pressed")
                            disabledImage:ButtonImage(@"autofill_next_inactive")
                                   target:_delegate
                                   action:@selector
                                   (selectNextElementWithButtonPress)
                                  enabled:NO
                                   inView:navView];
  [nextButton setAccessibilityLabel:l10n_util::GetNSString(
                                        IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)];

  [_delegate fetchPreviousAndNextElementsPresenceWithCompletionHandler:
                 ^(BOOL hasPreviousElement, BOOL hasNextElement) {
                   previousButton.enabled = hasPreviousElement;
                   nextButton.enabled = hasNextElement;
                 }];

  NSString* horizontalConstraint =
      @"H:|[separator1(==areaSeparatorWidth)][previousButton]["
      @"separator2(==buttonSeparatorWidth)][nextButton]";

  NSMutableArray<NSString*>* constraints =
      [NSMutableArray arrayWithObjects:@"V:|-(topPadding)-[separator1]|",
                                       @"V:|-(topPadding)-[previousButton]|",
                                       @"V:|-(topPadding)-[previousButton]|",
                                       @"V:|-(topPadding)-[separator2]|",
                                       @"V:|-(topPadding)-[nextButton]|", nil];

  NSMutableDictionary* views = [NSMutableDictionary
      dictionaryWithObjectsAndKeys:separator, @"separator1", previousButton,
                                   @"previousButton", internalSeparator,
                                   @"separator2", nextButton, @"nextButton",
                                   nil];

  if (ShouldShowCloseButton()) {
    // Add internal separator.
    UIView* internalSeparator2 =
        [[self class] createImageViewWithImageName:@"autofill_middle_sep"
                                            inView:navView];

    UIButton* closeButton =
        [self addKeyboardNavButtonWithNormalImage:ButtonImage(@"autofill_close")
                                     pressedImage:ButtonImage(
                                                      @"autofill_close_pressed")
                                    disabledImage:nil
                                           target:_delegate
                                           action:@selector
                                           (closeKeyboardWithButtonPress)
                                          enabled:YES
                                           inView:navView];
    [closeButton
        setAccessibilityLabel:l10n_util::GetNSString(
                                  IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)];

    [views setObject:internalSeparator2 forKey:@"internalSeparator2"];
    [views setObject:closeButton forKey:@"closeButton"];

    [constraints addObject:@"V:|-(topPadding)-[closeButton]|"];
    [constraints addObject:@"V:|-(topPadding)-[internalSeparator2]|"];

    horizontalConstraint =
        [horizontalConstraint stringByAppendingString:
                                  @"[internalSeparator2(buttonSeparatorWidth)]["
                                  @"closeButton]"];
  }

  [constraints addObject:[horizontalConstraint stringByAppendingString:@"|"]];

  NSDictionary* metrics = @{

    @"areaSeparatorWidth" : @(kNavigationAreaSeparatorWidth),
    @"buttonSeparatorWidth" : @(kNavigationButtonSeparatorWidth),
    @"topPadding" : @(1)
  };
  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);

  return navView;
}

- (UIButton*)addKeyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                    pressedImage:(UIImage*)pressedImage
                                   disabledImage:(UIImage*)disabledImage
                                          target:(id)target
                                          action:(SEL)action
                                         enabled:(BOOL)enabled
                                          inView:(UIView*)view {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button setBackgroundImage:normalImage forState:UIControlStateNormal];
  [button setBackgroundImage:pressedImage forState:UIControlStateSelected];
  [button setBackgroundImage:pressedImage forState:UIControlStateHighlighted];
  if (disabledImage)
    [button setBackgroundImage:disabledImage forState:UIControlStateDisabled];

  CALayer* layer = [button layer];
  layer.borderWidth = 0;
  layer.borderColor = [[UIColor blackColor] CGColor];
  button.enabled = enabled;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button.heightAnchor constraintEqualToAnchor:button.widthAnchor].active = YES;
  [view addSubview:button];
  return button;
}

+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName {
  UIImage* backgroundImage = StretchableImageNamed(imageName);

  UIImageView* backgroundImageView = [[UIImageView alloc] init];
  backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [backgroundImageView setImage:backgroundImage];
  [backgroundImageView setAlpha:kBackgroundColorAlpha];
  [view addSubview:backgroundImageView];
  [view sendSubviewToBack:backgroundImageView];
  AddSameConstraints(view, backgroundImageView);
}

+ (UIView*)createImageViewWithImageName:(NSString*)imageName
                                 inView:(UIView*)view {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:imageView];
  return imageView;
}

@end
