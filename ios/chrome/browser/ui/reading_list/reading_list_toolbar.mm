// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_toolbar.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Shadow opacity.
const CGFloat kShadowOpacity = 0.2f;
// Horizontal margin for the stack view content.
const CGFloat kHorizontalMargin = 8.0f;
// Horizontal spacing between the elements of the stack view.
const CGFloat kHorizontalSpacing = 8.0f;

typedef NS_ENUM(NSInteger, ButtonPositioning) { Leading, Centered, Trailing };

}  // namespace

@interface ReadingListToolbar ()

// Container for the edit button, preventing it to have the same width as the
// stack view.
@property(nonatomic, strong) UIView* editButtonContainer;
// Button that displays "Delete".
@property(nonatomic, strong) UIButton* deleteButton;
// Button that displays "Delete All Read".
@property(nonatomic, strong) UIButton* deleteAllButton;
// Button that displays "Cancel".
@property(nonatomic, strong) UIButton* cancelButton;
// Button that displays the mark options.
@property(nonatomic, strong) UIButton* markButton;
// Stack view for arranging the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Creates a button with a |title| and a style according to |destructive|.
- (UIButton*)buttonWithText:(NSString*)title
                destructive:(BOOL)isDestructive
                positioning:(ButtonPositioning)position;
// Set the mark button label to |text|.
- (void)setMarkButtonText:(NSString*)text;
// Updates the button labels to match an empty selection.
- (void)updateButtonsForEmptySelection;
// Updates the button labels to match a selection containing only read items.
- (void)updateButtonsForOnlyReadSelection;
// Updates the button labels to match a selection containing only unread items.
- (void)updateButtonsForOnlyUnreadSelection;
// Updates the button labels to match a selection containing unread and read
// items.
- (void)updateButtonsForOnlyMixedSelection;

@end

@implementation ReadingListToolbar

@synthesize editButtonContainer = _editButtonContainer;
@synthesize deleteButton = _deleteButton;
@synthesize deleteAllButton = _deleteAllButton;
@synthesize cancelButton = _cancelButton;
@synthesize stackView = _stackView;
@synthesize markButton = _markButton;
@synthesize state = _state;
@synthesize heightDelegate = _heightDelegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIButton* editButton = [self
        buttonWithText:l10n_util::GetNSString(IDS_IOS_READING_LIST_EDIT_BUTTON)
           destructive:NO
           positioning:Trailing];

    _deleteButton = [self buttonWithText:l10n_util::GetNSString(
                                             IDS_IOS_READING_LIST_DELETE_BUTTON)
                             destructive:YES
                             positioning:Leading];

    _deleteAllButton =
        [self buttonWithText:l10n_util::GetNSString(
                                 IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON)
                 destructive:YES
                 positioning:Leading];

    _markButton = [self buttonWithText:l10n_util::GetNSString(
                                           IDS_IOS_READING_LIST_MARK_ALL_BUTTON)
                           destructive:NO
                           positioning:Centered];

    _cancelButton = [self buttonWithText:l10n_util::GetNSString(
                                             IDS_IOS_READING_LIST_CANCEL_BUTTON)
                             destructive:NO
                             positioning:Trailing];

    [editButton addTarget:nil
                   action:@selector(enterEditingModePressed)
         forControlEvents:UIControlEventTouchUpInside];

    [_deleteButton addTarget:nil
                      action:@selector(deletePressed)
            forControlEvents:UIControlEventTouchUpInside];

    [_deleteAllButton addTarget:nil
                         action:@selector(deletePressed)
               forControlEvents:UIControlEventTouchUpInside];

    [_markButton addTarget:nil
                    action:@selector(markPressed)
          forControlEvents:UIControlEventTouchUpInside];

    [_cancelButton addTarget:nil
                      action:@selector(exitEditingModePressed)
            forControlEvents:UIControlEventTouchUpInside];

    _editButtonContainer = [[UIView alloc] initWithFrame:CGRectZero];
    [_editButtonContainer addSubview:editButton];
    editButton.translatesAutoresizingMaskIntoConstraints = NO;
    NSDictionary* views = @{ @"button" : editButton };
    NSArray* constraints = @[ @"V:|[button]|", @"H:[button]|" ];
    ApplyVisualConstraints(constraints, views);

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _editButtonContainer, _deleteButton, _deleteAllButton, _markButton,
      _cancelButton
    ]];
    _stackView.axis = UILayoutConstraintAxisHorizontal;
    _stackView.alignment = UIStackViewAlignmentFill;
    _stackView.distribution = UIStackViewDistributionFillEqually;
    _stackView.spacing = kHorizontalSpacing;

    [self addSubview:_stackView];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameSizeConstraint(_stackView, self);
    _stackView.layoutMargins =
        UIEdgeInsetsMake(0, kHorizontalMargin, 0, kHorizontalMargin);
    _stackView.layoutMarginsRelativeArrangement = YES;

    self.backgroundColor = [UIColor whiteColor];
    [[self layer] setShadowOpacity:kShadowOpacity];
    [self setEditing:NO];
  }
  return self;
}

#pragma mark Public Methods

- (void)setEditing:(BOOL)editing {
  self.editButtonContainer.hidden = editing;
  self.deleteButton.hidden = YES;
  self.deleteAllButton.hidden = !editing;
  self.cancelButton.hidden = !editing;
  self.markButton.hidden = !editing;

  [self updateHeight];
}

- (void)setState:(ReadingListToolbarState)state {
  switch (state) {
    case NoneSelected:
      [self updateButtonsForEmptySelection];
      break;
    case OnlyReadSelected:
      [self updateButtonsForOnlyReadSelection];
      break;
    case OnlyUnreadSelected:
      [self updateButtonsForOnlyUnreadSelection];
      break;
    case MixedItemsSelected:
      [self updateButtonsForOnlyMixedSelection];
      break;
  }
  _state = state;

  [self updateHeight];
}

- (void)setHasReadItem:(BOOL)hasRead {
  self.deleteAllButton.enabled = hasRead;
}

- (ActionSheetCoordinator*)actionSheetForMarkWithBaseViewController:
    (UIViewController*)viewController {
  return [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                           title:nil
                         message:nil
                            rect:self.markButton.bounds
                            view:self.markButton];
}

#pragma mark Private Methods

- (void)updateButtonsForEmptySelection {
  self.deleteAllButton.hidden = NO;
  self.deleteButton.hidden = YES;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_ALL_BUTTON)];
}

- (void)updateButtonsForOnlyReadSelection {
  self.deleteAllButton.hidden = YES;
  self.deleteButton.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)];
}

- (void)updateButtonsForOnlyUnreadSelection {
  self.deleteAllButton.hidden = YES;
  self.deleteButton.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_READ_BUTTON)];
}

- (void)updateButtonsForOnlyMixedSelection {
  self.deleteAllButton.hidden = YES;
  self.deleteButton.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_BUTTON)];
}

- (UIButton*)buttonWithText:(NSString*)title
                destructive:(BOOL)isDestructive
                positioning:(ButtonPositioning)position {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.contentEdgeInsets = UIEdgeInsetsMake(0, 8, 0, 8);
  [button setTitle:title forState:UIControlStateNormal];
  button.titleLabel.numberOfLines = 0;

  button.backgroundColor = [UIColor whiteColor];
  UIColor* textColor = isDestructive ? [[MDCPalette cr_redPalette] tint500]
                                     : [[MDCPalette cr_bluePalette] tint500];
  [button setTitleColor:textColor forState:UIControlStateNormal];
  [button setTitleColor:[UIColor lightGrayColor]
               forState:UIControlStateDisabled];
  [button setTitleColor:[textColor colorWithAlphaComponent:0.3]
               forState:UIControlStateHighlighted];
  [[button titleLabel]
      setFont:[[MDCTypography fontLoader] regularFontOfSize:14]];

  NSTextAlignment textAlignement;
  UIControlContentHorizontalAlignment horizontalAlignement;

  switch (position) {
    case Leading:
      if (UseRTLLayout()) {
        horizontalAlignement = UIControlContentHorizontalAlignmentRight;
        textAlignement = NSTextAlignmentRight;
      } else {
        horizontalAlignement = UIControlContentHorizontalAlignmentLeft;
        textAlignement = NSTextAlignmentLeft;
      }
      break;

    case Centered:
      horizontalAlignement = UIControlContentHorizontalAlignmentCenter;
      textAlignement = NSTextAlignmentCenter;
      break;

    case Trailing:
      if (UseRTLLayout()) {
        horizontalAlignement = UIControlContentHorizontalAlignmentLeft;
        textAlignement = NSTextAlignmentLeft;
      } else {
        horizontalAlignement = UIControlContentHorizontalAlignmentRight;
        textAlignement = NSTextAlignmentRight;
      }
      break;
  }

  button.contentHorizontalAlignment = horizontalAlignement;
  button.titleLabel.textAlignment = textAlignement;

  return button;
}

- (void)setMarkButtonText:(NSString*)text {
  [self.markButton setTitle:text forState:UIControlStateNormal];
}

- (void)updateHeight {
  for (UIButton* button in
       @[ _deleteButton, _deleteAllButton, _markButton, _cancelButton ]) {
    if (!button.hidden) {
      CGFloat rect = [button.titleLabel.text
                         cr_pixelAlignedSizeWithFont:button.titleLabel.font]
                         .width;
      if (rect > (self.frame.size.width - 2 * kHorizontalMargin -
                  2 * kHorizontalSpacing) /
                         3 -
                     16) {
        [self.heightDelegate toolbar:self onHeightChanged:ExpandedHeight];
        return;
      }
    }
  }
  [self.heightDelegate toolbar:self onHeightChanged:NormalHeight];
}

@end
