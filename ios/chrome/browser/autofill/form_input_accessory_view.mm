// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_delegate.h"
#import "ios/chrome/browser/ui/image_util.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The alpha value of the background color.
const CGFloat kBackgroundColorAlpha = 1.0;

// Horizontal margin around the custom view.
const CGFloat kCustomViewHorizontalMargin = 2;

// The width of the previous and next buttons.
const CGFloat kNavigationButtonWidth = 44;

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

// Returns the width of navigation view.
CGFloat GetNavigationViewWidth() {
  // The number of naviation buttons (includes close button if shown).
  NSUInteger numberNavigationButtons = 2;
  if (ShouldShowCloseButton())
    numberNavigationButtons++;
  return numberNavigationButtons * kNavigationButtonWidth +
         (numberNavigationButtons - 1) * kNavigationButtonSeparatorWidth +
         kNavigationAreaSeparatorWidth;
}

}  // namespace

@interface FormInputAccessoryView ()

// Initializes the view with the given |customView|.
// If the size of |rightFrame| is non-zero, the view will be split into two
// parts with |leftFrame| and |rightFrame|. Otherwise the Autofill view will
// be shown in |leftFrame|.
- (void)initializeViewWithCustomView:(UIView*)customView
                           leftFrame:(CGRect)leftFrame
                          rightFrame:(CGRect)rightFrame;

// Returns a view that shows navigation buttons in the |frame|.
- (UIView*)viewForNavigationButtonsInFrame:(CGRect)frame;

// Returns a navigation button for Autofill that has |normalImage| for state
// UIControlStateNormal, a |pressedImage| for states UIControlStateSelected and
// UIControlStateHighlighted, and an optional |disabledImage| for
// UIControlStateDisabled.
- (UIButton*)keyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                 pressedImage:(UIImage*)pressedImage
                                disabledImage:(UIImage*)disabledImage
                                       target:(id)target
                                       action:(SEL)action
                                      enabled:(BOOL)enabled
                                      originX:(CGFloat)originX
                                      originY:(CGFloat)originY
                                       height:(CGFloat)height;

// Adds a background image to |view|. The supplied image is stretched to fit the
// space by stretching the content its horizontal and vertical centers.
+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName;

// Adds an image view in |view| with an image named |imageName| at
// (|originX|, 0). The width is |width| and the height is the height of |view|.
+ (void)addImageViewWithImageName:(NSString*)imageName
                          originX:(CGFloat)originX
                          originY:(CGFloat)originY
                            width:(CGFloat)width
                           inView:(UIView*)view;

@end

@implementation FormInputAccessoryView {
  // The custom view that is displayed in the input accessory view.
  base::scoped_nsobject<UIView> _customView;

  // Delegate of this view.
  base::WeakNSProtocol<id<FormInputAccessoryViewDelegate>> _delegate;
}

- (instancetype)initWithFrame:(CGRect)frame
                     delegate:(id<FormInputAccessoryViewDelegate>)delegate
                   customView:(UIView*)customView
                    leftFrame:(CGRect)leftFrame
                   rightFrame:(CGRect)rightFrame {
  DCHECK(delegate);
  self = [super initWithFrame:frame];
  if (self) {
    _delegate.reset(delegate);
    _customView.reset([customView retain]);
    [self initializeViewWithCustomView:_customView
                             leftFrame:leftFrame
                            rightFrame:rightFrame];
  }
  return self;
}

- (instancetype)initWithFrame:(CGRect)frame customView:(UIView*)customView {
  self = [super initWithFrame:frame];
  if (self) {
    _customView.reset([customView retain]);
    customView.frame =
        CGRectMake(0, 0, CGRectGetWidth(frame), CGRectGetHeight(frame));
    [self addSubview:customView];

    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];
  }
  return self;
}

#pragma mark -
#pragma mark UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark -
#pragma mark Private Methods

- (void)initializeViewWithCustomView:(UIView*)customView
                           leftFrame:(CGRect)leftFrame
                          rightFrame:(CGRect)rightFrame {
  UIView* customViewContainer = [[[UIView alloc] init] autorelease];
  [self addSubview:customViewContainer];
  UIView* navView = [[[UIView alloc] init] autorelease];
  [self addSubview:navView];

  bool splitKeyboard = CGRectGetWidth(rightFrame) != 0;
  BOOL isRTL = base::i18n::IsRTL();

  // The computed frame for |customView|.
  CGRect customViewFrame;
  // Frame of a subview of |navView| in which navigation buttons will be shown.
  CGRect navFrame = CGRectZero;
  if (splitKeyboard) {
    NSString* navViewBackgroundImageName = nil;
    NSString* customViewContainerBackgroundImageName = nil;
    NSUInteger navFrameOriginX = 0;
    if (isRTL) {
      navView.frame = leftFrame;
      navViewBackgroundImageName = @"autofill_keyboard_background_left";
      customViewContainer.frame = rightFrame;
      customViewContainerBackgroundImageName =
          @"autofill_keyboard_background_right";
      // Navigation buttons will be shown on the left side.
      navFrameOriginX = 0;
    } else {
      customViewContainer.frame = leftFrame;
      customViewContainerBackgroundImageName =
          @"autofill_keyboard_background_left";
      navView.frame = rightFrame;
      navViewBackgroundImageName = @"autofill_keyboard_background_right";
      // Navigation buttons will be shown on the right side.
      navFrameOriginX =
          CGRectGetWidth(navView.frame) - GetNavigationViewWidth();
    }

    [[self class]
        addBackgroundImageInView:customViewContainer
                   withImageName:customViewContainerBackgroundImageName];
    [[self class] addBackgroundImageInView:navView
                             withImageName:navViewBackgroundImageName];

    // For RTL, the custom view is the right view; the padding should be at the
    // left side of this view. Otherwise, the custom view is the left view
    // and the space is at the right side.
    customViewFrame = CGRectMake(isRTL ? kCustomViewHorizontalMargin : 0, 0,
                                 CGRectGetWidth(customViewContainer.bounds) -
                                     kCustomViewHorizontalMargin,
                                 CGRectGetHeight(customViewContainer.bounds));
    navFrame = CGRectMake(navFrameOriginX, 0, GetNavigationViewWidth(),
                          CGRectGetHeight(navView.frame));
  } else {
    NSUInteger navViewFrameOriginX = 0;
    NSUInteger customViewContainerFrameOrginX = 0;
    if (isRTL) {
      navViewFrameOriginX = kNavigationAreaSeparatorShadowWidth;
      customViewContainerFrameOrginX = GetNavigationViewWidth();
    } else {
      navViewFrameOriginX =
          CGRectGetWidth(leftFrame) - GetNavigationViewWidth();
    }

    customViewContainer.frame =
        CGRectMake(customViewContainerFrameOrginX, 0,
                   CGRectGetWidth(leftFrame) - GetNavigationViewWidth() +
                       kNavigationAreaSeparatorShadowWidth,
                   CGRectGetHeight(leftFrame));
    navView.frame = CGRectMake(navViewFrameOriginX, 0, GetNavigationViewWidth(),
                               CGRectGetHeight(leftFrame));

    customViewFrame = customViewContainer.bounds;
    navFrame = navView.bounds;
    [[self class] addBackgroundImageInView:self
                             withImageName:@"autofill_keyboard_background"];
  }

  [customView setFrame:customViewFrame];
  [customViewContainer addSubview:customView];
  [navView addSubview:[self viewForNavigationButtonsInFrame:navFrame]];
}

UIImage* ButtonImage(NSString* name) {
  UIImage* rawImage = [UIImage imageNamed:name];
  return StretchableImageFromUIImage(rawImage, 1, 0);
}

- (UIView*)viewForNavigationButtonsInFrame:(CGRect)frame {
  UIView* navView = [[[UIView alloc] initWithFrame:frame] autorelease];

  BOOL isRTL = base::i18n::IsRTL();

  // Vertical space is left for a dividing line.
  CGFloat firstRow = 1;

  CGFloat currentX = 0;

  // Navigation view is at the right side if not RTL. Add a left separator in
  // this case.
  if (!isRTL) {
    [[self class] addImageViewWithImageName:@"autofill_left_sep"
                                    originX:currentX
                                    originY:firstRow
                                      width:kNavigationAreaSeparatorWidth
                                     inView:navView];
    currentX = kNavigationAreaSeparatorWidth;
  }

  UIButton* previousButton = [self
      keyboardNavButtonWithNormalImage:ButtonImage(@"autofill_prev")
                          pressedImage:ButtonImage(@"autofill_prev_pressed")
                         disabledImage:ButtonImage(@"autofill_prev_inactive")
                                target:_delegate
                                action:@selector(
                                           selectPreviousElementWithButtonPress)
                               enabled:NO
                               originX:currentX
                               originY:firstRow
                                height:CGRectGetHeight(frame)];
  [previousButton
      setAccessibilityLabel:l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_ACCNAME_PREVIOUS_FIELD)];
  [navView addSubview:previousButton];
  currentX += kNavigationButtonWidth;

  // Add internal separator.
  [[self class] addImageViewWithImageName:@"autofill_middle_sep"
                                  originX:currentX
                                  originY:firstRow
                                    width:kNavigationButtonSeparatorWidth
                                   inView:navView];
  currentX += kNavigationButtonSeparatorWidth;

  UIButton* nextButton = [self
      keyboardNavButtonWithNormalImage:ButtonImage(@"autofill_next")
                          pressedImage:ButtonImage(@"autofill_next_pressed")
                         disabledImage:ButtonImage(@"autofill_next_inactive")
                                target:_delegate
                                action:@selector(
                                           selectNextElementWithButtonPress)
                               enabled:NO
                               originX:currentX
                               originY:firstRow
                                height:CGRectGetHeight(frame)];
  [nextButton setAccessibilityLabel:l10n_util::GetNSString(
                                        IDS_IOS_AUTOFILL_ACCNAME_NEXT_FIELD)];
  [navView addSubview:nextButton];
  currentX += kNavigationButtonWidth;

  [_delegate fetchPreviousAndNextElementsPresenceWithCompletionHandler:
                 ^(BOOL hasPreviousElement, BOOL hasNextElement) {
                   previousButton.enabled = hasPreviousElement;
                   nextButton.enabled = hasNextElement;
                 }];

  if (ShouldShowCloseButton()) {
    // Add internal separator.
    [[self class] addImageViewWithImageName:@"autofill_middle_sep"
                                    originX:currentX
                                    originY:firstRow
                                      width:kNavigationButtonSeparatorWidth
                                     inView:navView];
    currentX += kNavigationButtonSeparatorWidth;

    UIButton* closeButton = [self
        keyboardNavButtonWithNormalImage:ButtonImage(@"autofill_close")
                            pressedImage:ButtonImage(@"autofill_close_pressed")
                           disabledImage:nil
                                  target:_delegate
                                  action:@selector(closeKeyboardWithButtonPress)
                                 enabled:YES
                                 originX:currentX
                                 originY:firstRow
                                  height:CGRectGetHeight(frame)];
    [closeButton
        setAccessibilityLabel:l10n_util::GetNSString(
                                  IDS_IOS_AUTOFILL_ACCNAME_HIDE_KEYBOARD)];
    [navView addSubview:closeButton];
    currentX += kNavigationButtonWidth;
  }

  // Navigation view is at the left side for RTL. Add a right separator in
  // this case.
  if (isRTL) {
    [[self class] addImageViewWithImageName:@"autofill_right_sep"
                                    originX:currentX
                                    originY:firstRow
                                      width:kNavigationAreaSeparatorWidth
                                     inView:navView];
  }

  return navView;
}

- (UIButton*)keyboardNavButtonWithNormalImage:(UIImage*)normalImage
                                 pressedImage:(UIImage*)pressedImage
                                disabledImage:(UIImage*)disabledImage
                                       target:(id)target
                                       action:(SEL)action
                                      enabled:(BOOL)enabled
                                      originX:(CGFloat)originX
                                      originY:(CGFloat)originY
                                       height:(CGFloat)height {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];

  button.frame =
      CGRectMake(originX, originY, kNavigationButtonWidth, height - originY);

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
  return button;
}

+ (void)addBackgroundImageInView:(UIView*)view
                   withImageName:(NSString*)imageName {
  UIImage* backgroundImage = StretchableImageNamed(imageName);

  UIImageView* backgroundImageView =
      [[[UIImageView alloc] initWithFrame:view.bounds] autorelease];
  [backgroundImageView setImage:backgroundImage];
  [backgroundImageView setAlpha:kBackgroundColorAlpha];
  [view addSubview:backgroundImageView];
  [view sendSubviewToBack:backgroundImageView];
}

+ (void)addImageViewWithImageName:(NSString*)imageName
                          originX:(CGFloat)originX
                          originY:(CGFloat)originY
                            width:(CGFloat)width
                           inView:(UIView*)view {
  UIImage* image =
      StretchableImageFromUIImage([UIImage imageNamed:imageName], 0, 0);
  base::scoped_nsobject<UIImageView> imageView(
      [[UIImageView alloc] initWithImage:image]);
  [imageView setFrame:CGRectMake(originX, originY, width,
                                 CGRectGetHeight(view.bounds) - originY)];
  [view addSubview:imageView];
}

@end
