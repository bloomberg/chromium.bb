// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_navigation_bar.h"

#import <QuartzCore/QuartzCore.h>

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_extended_button.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kButtonHeight = 24;
const CGFloat kButtonWidth = 24;

// The height of the content, not including possible extra space for the status
// bar.
const CGFloat kContentHeight = 56;
// The distance between buttons.
const CGFloat kInterButtonMargin = 24;
};  // namespace

@interface BookmarkNavigationBar () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkNavigationBar;
}
@property(nonatomic, retain) BookmarkExtendedButton* cancelButton;
// All subviews are added to |contentView|, which allows easy repositioning of
// the content to account for iOS 6 and iOS 7+ layout differences.
@property(nonatomic, retain) UIView* contentView;
@property(nonatomic, retain) BookmarkExtendedButton* backButton;
@property(nonatomic, retain) BookmarkExtendedButton* editButton;
@property(nonatomic, retain) BookmarkExtendedButton* menuButton;
@property(nonatomic, retain) UILabel* titleLabel;
@end

@implementation BookmarkNavigationBar
@synthesize backButton = _backButton;
@synthesize cancelButton = _cancelButton;
@synthesize contentView = _contentView;
@synthesize editButton = _editButton;
@synthesize menuButton = _menuButton;
@synthesize titleLabel = _titleLabel;

- (id)initWithFrame:(CGRect)outerFrame {
  self = [super initWithFrame:outerFrame];
  if (self) {
    _propertyReleaser_BookmarkNavigationBar.Init(self,
                                                 [BookmarkNavigationBar class]);

    self.backgroundColor = bookmark_utils_ios::mainBackgroundColor();
    self.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                            UIViewAutoresizingFlexibleWidth;

    // Position the content view at the bottom of |self|.
    CGFloat contentY = CGRectGetHeight(outerFrame) - kContentHeight;
    self.contentView = base::scoped_nsobject<UIView>([[UIView alloc]
        initWithFrame:CGRectMake(0, contentY, CGRectGetWidth(outerFrame),
                                 kContentHeight)]);
    [self addSubview:self.contentView];
    self.contentView.backgroundColor = [UIColor clearColor];
    self.contentView.autoresizingMask =
        UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleWidth;
    CGRect contentViewBounds = self.contentView.bounds;

    base::scoped_nsobject<UILabel> label([[UILabel alloc] init]);
    self.titleLabel = label;
    self.titleLabel.textColor = [UIColor colorWithWhite:68 / 255.0 alpha:1.0];
    self.titleLabel.backgroundColor = [UIColor clearColor];
    self.titleLabel.font = [MDCTypography titleFont];
    [self.contentView addSubview:self.titleLabel];

    // The buttons should be on the right side of the screen.
    CGFloat buttonVerticalMargin =
        floor((CGRectGetHeight(contentViewBounds) - kButtonHeight) / 2);
    CGFloat buttonSideMargin = buttonVerticalMargin;
    if (IsIPadIdiom())
      buttonSideMargin = kInterButtonMargin;

    CGFloat buttonX = CGRectGetWidth(contentViewBounds) - buttonSideMargin;

    if (!IsIPadIdiom()) {
      base::scoped_nsobject<BookmarkExtendedButton> button(
          [[BookmarkExtendedButton alloc] initWithFrame:CGRectZero]);

      self.cancelButton = button;
      self.cancelButton.autoresizingMask =
          UIViewAutoresizingFlexibleLeadingMargin();
      self.cancelButton.backgroundColor = [UIColor clearColor];
      [self.cancelButton
          setTitle:l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
                       .uppercaseString
          forState:UIControlStateNormal];
      [self.cancelButton
          setTitleColor:[UIColor colorWithWhite:68 / 255.0 alpha:1.0]
               forState:UIControlStateNormal];
      [[self.cancelButton titleLabel] setFont:[MDCTypography buttonFont]];
      self.cancelButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
      self.cancelButton.accessibilityIdentifier = @"Exit";

      // Size the button.
      [self.cancelButton sizeToFit];
      buttonX = buttonX - CGRectGetWidth(self.cancelButton.bounds);
      LayoutRect cancelButtonLayout = LayoutRectMake(
          buttonX, CGRectGetWidth(self.bounds), buttonVerticalMargin,
          CGRectGetWidth(self.cancelButton.bounds), kButtonHeight);
      self.cancelButton.frame = LayoutRectGetRect(cancelButtonLayout);
      self.cancelButton.extendedEdges =
          UIEdgeInsetsMake(buttonVerticalMargin, kInterButtonMargin / 2.0,
                           buttonVerticalMargin, buttonSideMargin);

      [self.contentView addSubview:self.cancelButton];

      buttonX = buttonX - kInterButtonMargin;
    }

    CGRect buttonFrame = LayoutRectGetRect(
        LayoutRectMake(buttonX - kButtonWidth, CGRectGetWidth(self.bounds),
                       buttonVerticalMargin, kButtonWidth, kButtonHeight));
    UIEdgeInsets buttonInsets =
        UIEdgeInsetsMake(buttonVerticalMargin, kInterButtonMargin / 2.0,
                         buttonVerticalMargin, kInterButtonMargin / 2.0);

    base::scoped_nsobject<BookmarkExtendedButton> editButton(
        [[BookmarkExtendedButton alloc] initWithFrame:buttonFrame]);
    self.editButton = editButton;
    self.editButton.extendedEdges = buttonInsets;
    self.editButton.autoresizingMask =
        UIViewAutoresizingFlexibleLeadingMargin();
    self.editButton.backgroundColor = [UIColor clearColor];
    UIImage* editImage = [UIImage imageNamed:@"bookmark_gray_edit"];
    [self.editButton setBackgroundImage:editImage
                               forState:UIControlStateNormal];
    self.editButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_EDIT_BUTTON_LABEL);
    self.editButton.accessibilityIdentifier = @"Edit_navigation_bar";
    [self.contentView addSubview:self.editButton];

    CGRect leftButtonFrame = LayoutRectGetRect(
        LayoutRectMake(buttonSideMargin, CGRectGetWidth(self.bounds),
                       buttonVerticalMargin, kButtonWidth, kButtonHeight));
    UIEdgeInsets leftButtonInsets =
        UIEdgeInsetsMake(buttonVerticalMargin, buttonSideMargin,
                         buttonVerticalMargin, buttonSideMargin);

    base::scoped_nsobject<BookmarkExtendedButton> menuButton(
        [[BookmarkExtendedButton alloc] initWithFrame:leftButtonFrame]);
    self.menuButton = menuButton;
    self.menuButton.extendedEdges = leftButtonInsets;
    self.menuButton.autoresizingMask =
        UIViewAutoresizingFlexibleTrailingMargin();
    self.menuButton.backgroundColor = [UIColor clearColor];
    UIImage* menuImage = [UIImage imageNamed:@"bookmark_gray_menu"];
    [self.menuButton setBackgroundImage:menuImage
                               forState:UIControlStateNormal];
    self.menuButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_MENU_LABEL);
    self.menuButton.accessibilityIdentifier = @"Menu";
    [self.contentView addSubview:self.menuButton];
    self.menuButton.hidden = YES;
    base::scoped_nsobject<BookmarkExtendedButton> backButton(
        [[BookmarkExtendedButton alloc] initWithFrame:leftButtonFrame]);
    self.backButton = backButton;
    self.backButton.extendedEdges = leftButtonInsets;
    self.backButton.autoresizingMask =
        UIViewAutoresizingFlexibleTrailingMargin();
    self.backButton.backgroundColor = [UIColor clearColor];
    UIImage* backImage = [UIImage imageNamed:@"bookmark_gray_back"];
    [self.backButton setBackgroundImage:backImage
                               forState:UIControlStateNormal];
    self.backButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BACK_LABEL);
    self.backButton.accessibilityIdentifier = @"Parent";
    self.backButton.alpha = 0;
    [self.contentView addSubview:self.backButton];

    [self setNeedsLayout];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // When the alpha of the button is less than this amount, the title is
  // allowed to overrun the button. Generally, layoutSubviews should only be
  // called with buttons having an alpha of 0.0 or 1.0, so the value of this
  // constant doesn't matter too much.
  const CGFloat buttonAlphaForTitleOverrun = 0.1;

  LayoutOffset labelTrailing = 0;

  // The title should not overrun the cancel or edit buttons.
  if (self.editButton && self.editButton.alpha > buttonAlphaForTitleOverrun) {
    labelTrailing = CGRectGetTrailingLayoutOffsetInBoundingRect(
                        self.editButton.frame, self.contentView.bounds) +
                    CGRectGetWidth(self.editButton.frame) + kInterButtonMargin;
  } else if (self.cancelButton) {
    labelTrailing = CGRectGetTrailingLayoutOffsetInBoundingRect(
                        self.cancelButton.frame, self.contentView.bounds) +
                    CGRectGetWidth(self.cancelButton.frame) +
                    kInterButtonMargin;
  }

  CGRect leadingRect = self.menuButton.frame;
  if (!bookmark_utils_ios::bookmarkMenuIsInSlideInPanel() &&
      self.backButton.alpha < buttonAlphaForTitleOverrun) {
    leadingRect = self.backButton.frame;
  }

  LayoutOffset labelLeading = CGRectGetLeadingLayoutOffsetInBoundingRect(
                                  leadingRect, self.contentView.bounds) +
                              CGRectGetWidth(leadingRect) + kInterButtonMargin;

  CGFloat width =
      CGRectGetWidth(self.contentView.bounds) - (labelTrailing + labelLeading);

  self.titleLabel.frame = LayoutRectGetRect(
      LayoutRectMake(labelLeading, CGRectGetWidth(self.contentView.bounds), 0,
                     width, CGRectGetHeight(self.contentView.bounds)));

  if (bookmark_utils_ios::bookmarkMenuIsInSlideInPanel())
    self.menuButton.hidden = NO;
  else
    self.menuButton.hidden = YES;
}

+ (CGFloat)expectedContentViewHeight {
  return kContentHeight;
}

- (void)setCancelTarget:(id)target action:(SEL)action {
  [self.cancelButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
  DCHECK(!IsIPadIdiom());
}

- (void)setEditTarget:(id)target action:(SEL)action {
  [self.editButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setMenuTarget:(id)target action:(SEL)action {
  [self.menuButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setBackTarget:(id)target action:(SEL)action {
  [self.backButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setTitle:(NSString*)title {
  self.titleLabel.text = title;
  [self setNeedsLayout];
}

- (void)showEditButtonWithAnimationDuration:(CGFloat)duration {
  if (self.editButton.alpha == 1.0)
    return;
  if (duration == 0.0) {
    self.editButton.alpha = 1.0;
    [self setNeedsLayout];
    return;
  }
  [UIView animateWithDuration:duration
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.editButton.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        if (finished)
          [self setNeedsLayout];
      }];
}

- (void)hideEditButtonWithAnimationDuration:(CGFloat)duration {
  if (self.editButton.alpha == 0.0)
    return;
  if (duration == 0.0) {
    self.editButton.alpha = 0.0;
    [self setNeedsLayout];
    return;
  }
  [UIView animateWithDuration:duration
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.editButton.alpha = 0.0;
      }
      completion:^(BOOL finished) {
        if (finished) {
          [self setNeedsLayout];
        }
      }];
}

- (void)updateEditButtonVisibility:(CGFloat)visibility {
  self.editButton.alpha = visibility;
}

- (void)showMenuButtonInsteadOfBackButton:(CGFloat)duration {
  if (self.menuButton.alpha == 1.0 && self.backButton.alpha == 0.0)
    return;
  if (duration == 0.0) {
    self.menuButton.alpha = 1.0;
    self.backButton.alpha = 0.0;
    [self setNeedsLayout];
    return;
  }
  [UIView animateWithDuration:duration
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.menuButton.alpha = 1.0;
        self.backButton.alpha = 0.0;
      }
      completion:^(BOOL finished) {
        if (finished) {
          [self setNeedsLayout];
        }
      }];
}

- (void)showBackButtonInsteadOfMenuButton:(CGFloat)duration {
  if (self.menuButton.alpha == 0.0 && self.backButton.alpha == 1.0)
    return;
  if (duration == 0.0) {
    self.menuButton.alpha = 0.0;
    self.backButton.alpha = 1.0;
    [self setNeedsLayout];
    return;
  }
  [UIView animateWithDuration:duration
      delay:0
      options:UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        self.menuButton.alpha = 0.0;
        self.backButton.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        if (finished) {
          [self setNeedsLayout];
        }
      }];
}

@end
