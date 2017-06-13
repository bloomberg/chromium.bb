// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/new_keyboard_accessory_view.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/fancy_ui/colored_button.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewKeyboardAccessoryView ()

@property(nonatomic, weak) id<KeyboardAccessoryViewDelegate> delegate;

// Called when a keyboard shortcut button is pressed.
- (void)keyboardButtonPressed:(NSString*)title;
// Creates a button shortcut for |title|.
- (UIView*)shortcutButtonWithTitle:(NSString*)title;
// Creates a button with an icon based on |iconName|.
- (UIButton*)iconButton:(NSString*)iconName;

@end

@implementation NewKeyboardAccessoryView

// Unused by this implementation of |KeyboardAccessoryViewProtocol|.
@synthesize mode = _mode;

@synthesize delegate = _delegate;

- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                       delegate:(id<KeyboardAccessoryViewDelegate>)delegate {
  const CGFloat kButtonMinSizeX = 61.0;
  const CGFloat kButtonMinSizeXCompact = 32.0;
  const CGFloat kButtonSizeY = 62.0;
  const CGFloat kButtonSizeYCompact = 30.0;
  const CGFloat kBetweenButtonSpacing = 15.0;
  const CGFloat kBetweenButtonSpacingCompact = 6.0;
  const CGFloat kSeparatorAlpha = 0.1;
  const CGFloat kViewHeight = 70.0;
  const CGFloat kViewHeightCompact = 43.0;
  const BOOL isCompact = IsCompact();

  CGFloat width = [[UIScreen mainScreen] bounds].size.width;
  CGFloat height = isCompact ? kViewHeightCompact : kViewHeight;
  CGRect frame = CGRectMake(0.0, 0.0, width, height);

  self = [super initWithFrame:frame inputViewStyle:UIInputViewStyleKeyboard];
  if (self) {
    _delegate = delegate;

    // Create and add stackview filled with the shortcut buttons.
    UIStackView* stackView = [[UIStackView alloc] init];
    [stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    stackView.spacing =
        isCompact ? kBetweenButtonSpacingCompact : kBetweenButtonSpacing;
    for (NSString* title in buttonTitles) {
      UIView* button = [self shortcutButtonWithTitle:title];
      [button setTranslatesAutoresizingMaskIntoConstraints:NO];
      CGFloat buttonMinWidth =
          isCompact ? kButtonMinSizeXCompact : kButtonMinSizeX;
      [button.widthAnchor constraintGreaterThanOrEqualToConstant:buttonMinWidth]
          .active = YES;
      CGFloat buttonHeight = isCompact ? kButtonSizeYCompact : kButtonSizeY;
      [button.heightAnchor constraintEqualToConstant:buttonHeight].active = YES;
      [stackView addArrangedSubview:button];
    }
    [self addSubview:stackView];

    // Create and add buttons for voice and camera search.
    UIButton* cameraButton = [self iconButton:@"qr_scanner_keyboard_accessory"];
    [cameraButton
               addTarget:_delegate
                  action:@selector(keyboardAccessoryCameraSearchTouchUpInside)
        forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:cameraButton];

    UIButton* voiceSearchButton =
        [self iconButton:@"voice_icon_keyboard_accessory"];
    [self addSubview:voiceSearchButton];
    [voiceSearchButton
               addTarget:_delegate
                  action:@selector(keyboardAccessoryVoiceSearchTouchDown)
        forControlEvents:UIControlEventTouchDown];
    [voiceSearchButton
               addTarget:_delegate
                  action:@selector(keyboardAccessoryVoiceSearchTouchUpInside)
        forControlEvents:UIControlEventTouchUpInside];

    // Create and add 1 pixel high separator view.
    UIView* separator = [[UIView alloc] init];
    [separator setTranslatesAutoresizingMaskIntoConstraints:NO];
    [separator
        setBackgroundColor:[UIColor colorWithWhite:0 alpha:kSeparatorAlpha]];
    [separator.heightAnchor
        constraintEqualToConstant:1.0 / [[UIScreen mainScreen] scale]]
        .active = YES;
    [self addSubview:separator];

    // Position all the views.
    NSDictionary* viewsDictionary = @{
      @"cameraButton" : cameraButton,
      @"voiceSearch" : voiceSearchButton,
      @"stackView" : stackView,
      @"separator" : separator,
    };
    NSArray* constraints = @[
      @"H:|-8-[cameraButton]-8-[voiceSearch]-(>=16)-[stackView]",
      @"H:|-0-[separator]-0-|",
      @"V:[separator]-0-|",
    ];
    ApplyVisualConstraintsWithOptions(constraints, viewsDictionary,
                                      LayoutOptionForRTLSupport(), self);
    AddSameCenterYConstraint(self, cameraButton);
    AddSameCenterYConstraint(self, voiceSearchButton);
    AddSameCenterYConstraint(self, stackView);
    // The following constraint is supposed to break if there's not enough
    // space to center the stack view.
    NSLayoutConstraint* horizontallyCenterStackViewConstraint =
        [self.centerXAnchor constraintEqualToAnchor:stackView.centerXAnchor];
    horizontallyCenterStackViewConstraint.priority =
        UILayoutPriorityDefaultHigh;
    horizontallyCenterStackViewConstraint.active = YES;
  }
  return self;
}

- (UIView*)shortcutButtonWithTitle:(NSString*)title {
  const CGFloat kCornerRadius = 4.0;
  const CGFloat kAlphaStateNormal = 0.1;
  const CGFloat kAlphaStateHighlighted = 0.2;
  const CGFloat kHorizontalEdgeInset = 8;
  const CGFloat kIpadButtonTitleFontSize = 20.0;
  const CGFloat kIphoneButtonTitleFontSize = 15.0;

  ColoredButton* button = [ColoredButton buttonWithType:UIButtonTypeCustom];
  UIFont* font = nil;

  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];

  [button setBackgroundColor:UIColorFromRGB(0, kAlphaStateNormal)
                    forState:UIControlStateNormal];
  [button setBackgroundColor:UIColorFromRGB(0, kAlphaStateHighlighted)
                    forState:UIControlStateHighlighted];

  button.layer.cornerRadius = kCornerRadius;
  button.contentEdgeInsets =
      UIEdgeInsetsMake(0, kHorizontalEdgeInset, 0, kHorizontalEdgeInset);
  button.clipsToBounds = YES;

  if (IsIPadIdiom()) {
    font = [UIFont systemFontOfSize:kIpadButtonTitleFontSize];
  } else {
    font = [UIFont boldSystemFontOfSize:kIphoneButtonTitleFontSize];
  }

  [button.titleLabel setFont:font];

  [button addTarget:self
                action:@selector(keyboardButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  button.isAccessibilityElement = YES;
  [button setAccessibilityLabel:title];
  return button;
}

- (UIButton*)iconButton:(NSString*)iconName {
  const CGFloat kIconTintAlphaStateNormal = 0.45;
  const CGFloat kIconTintAlphaStateHighlighted = 0.6;

  UIColor* iconTintStateNormal = UIColorFromRGB(0, kIconTintAlphaStateNormal);
  UIColor* iconTintStateHighlighted =
      UIColorFromRGB(0, kIconTintAlphaStateHighlighted);

  ColoredButton* button = [ColoredButton buttonWithType:UIButtonTypeCustom];

  [button setTintColor:iconTintStateNormal forState:UIControlStateNormal];
  [button setTintColor:iconTintStateHighlighted
              forState:UIControlStateHighlighted];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [[UIImage imageNamed:iconName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [button setImage:icon forState:UIControlStateNormal];
  return button;
}

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

- (void)keyboardButtonPressed:(id)sender {
  UIButton* button = base::mac::ObjCCastStrict<UIButton>(sender);
  [[UIDevice currentDevice] playInputClick];
  [_delegate keyPressed:[button currentTitle]];
}

@end
