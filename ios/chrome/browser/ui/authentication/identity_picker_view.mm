// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_picker_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kIdentityPickerViewRadius = 8.;
// Sizes.
const CGFloat kIdentityAvatarSize = 40.;
const CGFloat kTitleFontSize = 14.;
const CGFloat kSubtitleFontSize = 12.;
const CGFloat kArrowDownSize = 24.;
// Distances/margins.
const CGFloat kTitleOffset = 2;
const CGFloat kArrowDownMargin = 12.;
const CGFloat kHorizontalAvatarMargin = 16.;
const CGFloat kVerticalMargin = 12.;
// Colors
const int kHeaderBackgroundColor = 0xf1f3f4;
const CGFloat kTitleTextColorAlpha = .87;
const CGFloat kSubtitleTextColorAlpha = .54;

}  // namespace

@interface IdentityPickerView ()

@property(nonatomic) UIImageView* avatarImageView;
@property(nonatomic) UILabel* title;
@property(nonatomic) UILabel* subtitle;
@property(nonatomic) MDCInkView* inkView;
@property(nonatomic) NSLayoutConstraint* titleConstraintForNameAndEmail;
@property(nonatomic) NSLayoutConstraint* titleConstraintForEmailOnly;

@end

@implementation IdentityPickerView

@synthesize avatarImageView = _avatarImageView;
@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize inkView = _inkView;
// Constraints when email and name are available.
@synthesize titleConstraintForNameAndEmail = _titleConstraintForNameAndEmail;
// Constraints when only the email is avaiable.
@synthesize titleConstraintForEmailOnly = _titleConstraintForEmailOnly;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = kIdentityPickerViewRadius;
    self.backgroundColor = UIColorFromRGB(kHeaderBackgroundColor);
    // Adding view elements inside.
    // Ink view.
    _inkView = [[MDCInkView alloc] initWithFrame:CGRectZero];
    _inkView.layer.cornerRadius = kIdentityPickerViewRadius;
    _inkView.inkStyle = MDCInkStyleBounded;
    _inkView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_inkView];

    // Avatar view.
    _avatarImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_avatarImageView];

    // Arrow down.
    UIImageView* arrowDownImageView =
        [[UIImageView alloc] initWithFrame:CGRectZero];
    arrowDownImageView.translatesAutoresizingMaskIntoConstraints = NO;
    arrowDownImageView.image =
        [UIImage imageNamed:@"identity_picker_view_arrow_down"];
    [self addSubview:arrowDownImageView];

    // Title.
    _title = [[UILabel alloc] initWithFrame:CGRectZero];
    _title.translatesAutoresizingMaskIntoConstraints = NO;
    _title.textColor = [UIColor colorWithWhite:0 alpha:kTitleTextColorAlpha];
    _title.font = [UIFont systemFontOfSize:kTitleFontSize];
    [self addSubview:_title];

    // Subtitle.
    _subtitle = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitle.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitle.textColor =
        [UIColor colorWithWhite:0 alpha:kSubtitleTextColorAlpha];
    _subtitle.font = [UIFont systemFontOfSize:kSubtitleFontSize];
    [self addSubview:_subtitle];

    // Layout constraints.
    NSDictionary* views = @{
      @"avatar" : _avatarImageView,
      @"title" : _title,
      @"subtitle" : _subtitle,
      @"arrow" : arrowDownImageView,
    };
    NSDictionary* metrics = @{
      @"ArMrg" : @(kArrowDownMargin),
      @"ArrowSize" : @(kArrowDownSize),
      @"AvatarSize" : @(kIdentityAvatarSize),
      @"HAvatMrg" : @(kHorizontalAvatarMargin),
      @"VMargin" : @(kVerticalMargin),
    };
    NSArray* constraints = @[
      // Horizontal constraints.
      @"H:|-(HAvatMrg)-[avatar]-(HAvatMrg)-[title]-(ArMrg)-[arrow]-(ArMrg)-|",
      // Vertical constraints.
      @"V:|-(>=VMargin)-[avatar]-(>=VMargin)-|",
      @"V:|-(>=VMargin)-[title]",
      @"V:[subtitle]-(>=VMargin)-|",
      // Size constraints.
      @"H:[avatar(AvatarSize)]",
      @"V:[avatar(AvatarSize)]",
      @"H:[arrow(ArrowSize)]",
      @"V:[arrow(ArrowSize)]",
    ];
    AddSameCenterYConstraint(self, _avatarImageView);
    AddSameCenterYConstraint(self, arrowDownImageView);
    ApplyVisualConstraintsWithMetrics(constraints, views, metrics);
    AddSameConstraintsToSides(_title, _subtitle,
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    _titleConstraintForNameAndEmail =
        [self.centerYAnchor constraintEqualToAnchor:_title.bottomAnchor
                                           constant:kTitleOffset];
    _titleConstraintForEmailOnly =
        [self.centerYAnchor constraintEqualToAnchor:_title.centerYAnchor];
    [self.centerYAnchor constraintEqualToAnchor:_subtitle.topAnchor
                                       constant:-kTitleOffset]
        .active = YES;
  }
  return self;
}

#pragma mark - Setter

- (void)setIdentityAvatar:(UIImage*)identityAvatar {
  _avatarImageView.image =
      CircularImageFromImage(identityAvatar, kIdentityAvatarSize);
}

- (void)setIdentityName:(NSString*)name email:(NSString*)email {
  DCHECK(email);
  if (!name || name.length == 0) {
    self.titleConstraintForNameAndEmail.active = NO;
    self.titleConstraintForEmailOnly.active = YES;
    self.subtitle.hidden = YES;
    self.title.text = email;
  } else {
    self.titleConstraintForEmailOnly.active = NO;
    self.titleConstraintForNameAndEmail.active = YES;
    self.subtitle.hidden = NO;
    self.title.text = name;
    self.subtitle.text = email;
  }
}

#pragma mark - Private

- (CGPoint)locationFromTouches:(NSSet*)touches {
  UITouch* touch = [touches anyObject];
  return [touch locationInView:self];
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.inkView startTouchBeganAnimationAtPoint:location completion:nil];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.inkView startTouchEndedAnimationAtPoint:location completion:nil];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  CGPoint location = [self locationFromTouches:touches];
  [self.inkView startTouchEndedAnimationAtPoint:location completion:nil];
}

@end
