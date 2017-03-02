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
@property(nonatomic, strong) UIView* deleteButtonContainer;
// Button that displays "Delete All Read".
@property(nonatomic, strong) UIButton* deleteAllButton;
@property(nonatomic, strong) UIView* deleteAllButtonContainer;
// Button that displays "Cancel".
@property(nonatomic, strong) UIButton* cancelButton;
@property(nonatomic, strong) UIView* cancelButtonContainer;
// Button that displays the mark options.
@property(nonatomic, strong) UIButton* markButton;
@property(nonatomic, strong) UIView* markButtonContainer;
// Stack view for arranging the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Creates a button with a |title| and a style according to |destructive|.
- (UIButton*)buttonWithText:(NSString*)title destructive:(BOOL)isDestructive;
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
@synthesize deleteButtonContainer = _deleteButtonContainer;
@synthesize deleteAllButton = _deleteAllButton;
@synthesize deleteAllButtonContainer = _deleteAllButtonContainer;
@synthesize cancelButton = _cancelButton;
@synthesize cancelButtonContainer = _cancelButtonContainer;
@synthesize stackView = _stackView;
@synthesize markButton = _markButton;
@synthesize markButtonContainer = _markButtonContainer;
@synthesize state = _state;
@synthesize heightDelegate = _heightDelegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    NSDictionary* views = nil;
    NSArray* constraints = nil;

    _deleteButton = [self buttonWithText:l10n_util::GetNSString(
                                             IDS_IOS_READING_LIST_DELETE_BUTTON)
                             destructive:YES];
    _deleteButton.translatesAutoresizingMaskIntoConstraints = NO;
    _deleteButtonContainer = [[UIView alloc] init];
    [_deleteButtonContainer addSubview:_deleteButton];
    views = @{ @"button" : _deleteButton };
    constraints = @[ @"V:|[button]|", @"H:|[button]" ];
    ApplyVisualConstraints(constraints, views);
    [_deleteButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_deleteButtonContainer.trailingAnchor]
        .active = YES;

    _deleteAllButton =
        [self buttonWithText:l10n_util::GetNSString(
                                 IDS_IOS_READING_LIST_DELETE_ALL_READ_BUTTON)
                 destructive:YES];
    _deleteAllButton.translatesAutoresizingMaskIntoConstraints = NO;
    _deleteAllButtonContainer = [[UIView alloc] init];
    [_deleteAllButtonContainer addSubview:_deleteAllButton];
    views = @{ @"button" : _deleteAllButton };
    constraints = @[ @"V:|[button]|", @"H:|[button]" ];
    ApplyVisualConstraints(constraints, views);
    [_deleteAllButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_deleteAllButtonContainer
                                              .trailingAnchor]
        .active = YES;

    _markButton = [self buttonWithText:l10n_util::GetNSString(
                                           IDS_IOS_READING_LIST_MARK_ALL_BUTTON)
                           destructive:NO];
    _markButton.translatesAutoresizingMaskIntoConstraints = NO;
    _markButtonContainer = [[UIView alloc] init];
    [_markButtonContainer addSubview:_markButton];
    views = @{ @"button" : _markButton };
    constraints = @[ @"V:|[button]|" ];
    ApplyVisualConstraints(constraints, views);
    [_markButton.centerXAnchor
        constraintEqualToAnchor:_markButtonContainer.centerXAnchor]
        .active = YES;
    [_markButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_markButtonContainer.trailingAnchor]
        .active = YES;
    [_markButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_markButtonContainer.leadingAnchor]
        .active = YES;

    _cancelButton = [self buttonWithText:l10n_util::GetNSString(
                                             IDS_IOS_READING_LIST_CANCEL_BUTTON)
                             destructive:NO];
    _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
    _cancelButtonContainer = [[UIView alloc] init];
    [_cancelButtonContainer addSubview:_cancelButton];
    views = @{ @"button" : _cancelButton };
    constraints = @[ @"V:|[button]|", @"H:[button]|" ];
    ApplyVisualConstraints(constraints, views);
    [_cancelButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_cancelButtonContainer
                                                 .leadingAnchor]
        .active = YES;

    UIButton* editButton = [self
        buttonWithText:l10n_util::GetNSString(IDS_IOS_READING_LIST_EDIT_BUTTON)
           destructive:NO];
    editButton.translatesAutoresizingMaskIntoConstraints = NO;
    _editButtonContainer = [[UIView alloc] initWithFrame:CGRectZero];
    [_editButtonContainer addSubview:editButton];
    views = @{ @"button" : editButton };
    constraints = @[ @"V:|[button]|", @"H:[button]|" ];
    ApplyVisualConstraints(constraints, views);

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

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _editButtonContainer, _deleteButtonContainer, _deleteAllButtonContainer,
      _markButtonContainer, _cancelButtonContainer
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
  self.deleteButtonContainer.hidden = YES;
  self.deleteAllButtonContainer.hidden = !editing;
  self.cancelButtonContainer.hidden = !editing;
  self.markButtonContainer.hidden = !editing;

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
  self.deleteAllButtonContainer.hidden = NO;
  self.deleteButtonContainer.hidden = YES;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_ALL_BUTTON)];
}

- (void)updateButtonsForOnlyReadSelection {
  self.deleteAllButtonContainer.hidden = YES;
  self.deleteButtonContainer.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_UNREAD_BUTTON)];
}

- (void)updateButtonsForOnlyUnreadSelection {
  self.deleteAllButtonContainer.hidden = YES;
  self.deleteButtonContainer.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_READ_BUTTON)];
}

- (void)updateButtonsForOnlyMixedSelection {
  self.deleteAllButtonContainer.hidden = YES;
  self.deleteButtonContainer.hidden = NO;
  [self setMarkButtonText:l10n_util::GetNSStringWithFixup(
                              IDS_IOS_READING_LIST_MARK_BUTTON)];
}

- (UIButton*)buttonWithText:(NSString*)title destructive:(BOOL)isDestructive {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.contentEdgeInsets = UIEdgeInsetsMake(0, 8, 0, 8);
  [button setTitle:title forState:UIControlStateNormal];
  button.titleLabel.numberOfLines = 3;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;

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

  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.titleLabel.textAlignment = NSTextAlignmentCenter;

  return button;
}

- (void)setMarkButtonText:(NSString*)text {
  [self.markButton setTitle:text forState:UIControlStateNormal];
}

- (void)updateHeight {
  for (UIButton* button in
       @[ _deleteButton, _deleteAllButton, _markButton, _cancelButton ]) {
    if (!button.superview.hidden) {
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
