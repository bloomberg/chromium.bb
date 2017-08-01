// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_keyboard_accessory_view.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarKeyboardAccessoryView ()

@property(nonatomic, retain) NSArray<NSString*>* buttonTitles;
@property(nonatomic, weak) id<ToolbarAssistiveKeyboardDelegate> delegate;

// Called when a keyboard shortcut button is pressed.
- (void)keyboardButtonPressed:(NSString*)title;
// Creates a button shortcut for |title|.
- (UIView*)shortcutButtonWithTitle:(NSString*)title;

@end

@implementation ToolbarKeyboardAccessoryView

@synthesize buttonTitles = _buttonTitles;
@synthesize delegate = _delegate;

- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                       delegate:(id<ToolbarAssistiveKeyboardDelegate>)delegate {
  const CGFloat kViewHeight = 44.0;
  CGFloat width = [[UIScreen mainScreen] bounds].size.width;
  // TODO(crbug.com/734512): Have the creator of the view define the size.
  CGRect frame = CGRectMake(0.0, 0.0, width, kViewHeight);

  self = [super initWithFrame:frame inputViewStyle:UIInputViewStyleKeyboard];
  if (self) {
    _buttonTitles = buttonTitles;
    _delegate = delegate;
    [self addSubviews];
  }
  return self;
}

- (void)addSubviews {
  if (!self.subviews.count)
    return;

  const CGFloat kButtonMinWidth = 34.0;
  const CGFloat kButtonHeight = 34.0;
  const CGFloat kBetweenShortcutButtonSpacing = 5.0;
  const CGFloat kBetweenSearchButtonSpacing = 12.0;
  const CGFloat kHorizontalMargin = 10.0;

  // Create and add stackview filled with the shortcut buttons.
  UIStackView* shortcutStackView = [[UIStackView alloc] init];
  shortcutStackView.translatesAutoresizingMaskIntoConstraints = NO;
  shortcutStackView.spacing = kBetweenShortcutButtonSpacing;
  for (NSString* title in self.buttonTitles) {
    UIView* button = [self shortcutButtonWithTitle:title];
    [button setTranslatesAutoresizingMaskIntoConstraints:NO];
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:kButtonMinWidth]
        .active = YES;
    [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;
    [shortcutStackView addArrangedSubview:button];
  }
  [self addSubview:shortcutStackView];

  // Create and add a stackview containing the leading assistive buttons, i.e.
  // Voice search and camera search.
  NSArray<UIButton*>* leadingButtons =
      ToolbarAssistiveKeyboardLeadingButtons(_delegate);
  UIStackView* searchStackView = [[UIStackView alloc] init];
  searchStackView.translatesAutoresizingMaskIntoConstraints = NO;
  searchStackView.spacing = kBetweenSearchButtonSpacing;
  for (UIButton* button in leadingButtons) {
    [searchStackView addArrangedSubview:button];
  }
  [self addSubview:searchStackView];

  // Position the stack views.
  NSArray* constraints = @[
    @"H:|-horizontalMargin-[searchStackView]-(>=0)-[shortcutStackView]",
    @"[shortcutStackView]-horizontalMargin-|",
  ];
  NSDictionary* viewsDictionary = @{
    @"searchStackView" : searchStackView,
    @"shortcutStackView" : shortcutStackView,
  };
  NSDictionary* metrics = @{
    @"horizontalMargin" : @(kHorizontalMargin),
  };
  ApplyVisualConstraintsWithMetrics(constraints, viewsDictionary, metrics);
  AddSameCenterYConstraint(searchStackView, self);
  AddSameCenterYConstraint(shortcutStackView, self);
}

- (UIView*)shortcutButtonWithTitle:(NSString*)title {
  const CGFloat kHorizontalEdgeInset = 8;
  const CGFloat kButtonTitleFontSize = 16.0;
  UIColor* kTitleColorStateNormal = [UIColor colorWithWhite:0.0 alpha:1.0];
  UIColor* kTitleColorStateHighlighted = [UIColor colorWithWhite:0.0 alpha:0.3];

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTitleColor:kTitleColorStateNormal forState:UIControlStateNormal];
  [button setTitleColor:kTitleColorStateHighlighted
               forState:UIControlStateHighlighted];

  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(0, kHorizontalEdgeInset, 0, kHorizontalEdgeInset);
  button.clipsToBounds = YES;
  [button.titleLabel setFont:[UIFont systemFontOfSize:kButtonTitleFontSize
                                               weight:UIFontWeightMedium]];

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
