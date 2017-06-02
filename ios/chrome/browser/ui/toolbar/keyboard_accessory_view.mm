// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_accessory_view.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface KeyboardAccessoryView ()

@property(nonatomic, weak) id<KeyboardAccessoryViewDelegate> delegate;
@property(nonatomic, retain) UIButton* voiceSearchButton;

// Creates a button with the same appearance as a keyboard key.
- (UIView*)keyboardButtonWithTitle:(NSString*)title frame:(CGRect)frame;
// Called when a keyboard shortcut button is pressed.
- (void)keyboardButtonPressed:(NSString*)title;

@end

@implementation KeyboardAccessoryView

@synthesize mode = _mode;
@synthesize delegate = _delegate;
@synthesize voiceSearchButton = _voiceSearchButton;

- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                       delegate:(id<KeyboardAccessoryViewDelegate>)delegate {
  const CGFloat kViewHeight = 70.0;
  const CGFloat kViewHeightCompact = 43.0;
  const CGFloat kButtonInset = 5.0;
  const CGFloat kButtonSizeX = 61.0;
  const CGFloat kButtonSizeXCompact = 46.0;
  const CGFloat kButtonSizeY = 62.0;
  const CGFloat kButtonSizeYCompact = 35.0;
  const CGFloat kBetweenButtonSpacing = 15.0;
  const CGFloat kBetweenButtonSpacingCompact = 7.0;
  const BOOL isCompact = IsCompact();

  CGFloat width = [[UIScreen mainScreen] bounds].size.width;
  CGFloat height = isCompact ? kViewHeightCompact : kViewHeight;
  CGRect frame = CGRectMake(0.0, 0.0, width, height);

  self = [super initWithFrame:frame inputViewStyle:UIInputViewStyleKeyboard];
  if (self) {
    _delegate = delegate;

    // Center buttons in available space by placing them within a parent view
    // that auto-centers.
    CGFloat betweenButtonSpacing =
        isCompact ? kBetweenButtonSpacingCompact : kBetweenButtonSpacing;
    const CGFloat buttonWidth = isCompact ? kButtonSizeXCompact : kButtonSizeX;

    CGFloat totalWidth = (buttonTitles.count * buttonWidth) +
                         ((buttonTitles.count - 1) * betweenButtonSpacing);
    CGFloat indent = floor((width - totalWidth) / 2.0);
    if (indent < kButtonInset)
      indent = kButtonInset;
    CGRect parentViewRect = CGRectMake(indent, 0.0, totalWidth, height);
    UIView* parentView = [[UIView alloc] initWithFrame:parentViewRect];
    [parentView setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin |
                                    UIViewAutoresizingFlexibleRightMargin];
    [self addSubview:parentView];

    // Create the shortcut buttons, starting at the left edge of |parentView|.
    CGRect currentFrame =
        CGRectMake(0.0, kButtonInset, buttonWidth,
                   isCompact ? kButtonSizeYCompact : kButtonSizeY);

    for (NSString* title in buttonTitles) {
      UIView* button = [self keyboardButtonWithTitle:title frame:currentFrame];
      [parentView addSubview:button];
      currentFrame.origin.x =
          CGRectGetMaxX(currentFrame) + betweenButtonSpacing;
    }

    // Create the voice search button and add it over the text buttons.
    _voiceSearchButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_voiceSearchButton setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    SetA11yLabelAndUiAutomationName(
        _voiceSearchButton, IDS_IOS_ACCNAME_VOICE_SEARCH, @"Voice Search");
    UIImage* voiceRow = [UIImage imageNamed:@"custom_row_voice"];
    UIImage* voiceRowPressed = [UIImage imageNamed:@"custom_row_voice_pressed"];
    [_voiceSearchButton setBackgroundImage:voiceRow
                                  forState:UIControlStateNormal];
    [_voiceSearchButton setBackgroundImage:voiceRowPressed
                                  forState:UIControlStateHighlighted];

    UIImage* voiceIcon = [UIImage imageNamed:@"voice_icon_keyboard_accessory"];
    [_voiceSearchButton setAdjustsImageWhenHighlighted:NO];
    [_voiceSearchButton setImage:voiceIcon forState:UIControlStateNormal];
    [_voiceSearchButton setFrame:[self bounds]];

    [_voiceSearchButton
               addTarget:delegate
                  action:@selector(keyboardAccessoryVoiceSearchTouchDown)
        forControlEvents:UIControlEventTouchDown];
    [_voiceSearchButton
               addTarget:delegate
                  action:@selector(keyboardAccessoryVoiceSearchTouchUpInside)
        forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:_voiceSearchButton];
  }

  return self;
}

- (void)setMode:(KeyboardAccessoryViewMode)mode {
  _mode = mode;
  switch (mode) {
    case VOICE_SEARCH:
      [_voiceSearchButton setHidden:NO];
      break;
    case KEY_SHORTCUTS:
      [_voiceSearchButton setHidden:YES];
      break;
  }
}

- (UIView*)keyboardButtonWithTitle:(NSString*)title frame:(CGRect)frame {
  const CGFloat kIpadButtonTitleFontSize = 20.0;
  const CGFloat kIphoneButtonTitleFontSize = 15.0;

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  UIFont* font = nil;
  UIImage* backgroundImage = nil;
  if (IsIPadIdiom()) {
    font = GetUIFont(FONT_HELVETICA, false, kIpadButtonTitleFontSize);
  } else {
    font = GetUIFont(FONT_HELVETICA, true, kIphoneButtonTitleFontSize);
  }
  backgroundImage = [UIImage imageNamed:@"keyboard_button"];

  button.frame = frame;
  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
  [button.titleLabel setFont:font];
  [button setBackgroundImage:backgroundImage forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(keyboardButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  button.isAccessibilityElement = YES;
  [button setAccessibilityLabel:title];
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
