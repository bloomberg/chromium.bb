// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/clear_browsing_bar.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Shadow opacity for the clear browsing bar.
CGFloat kShadowOpacity = 0.2f;
// Horizontal margin for the contents of ClearBrowsingBar.
CGFloat kHorizontalMargin = 8.0f;
// Enum to specify button position in the clear browsing bar.
typedef NS_ENUM(BOOL, ButtonPlacement) { Leading, Trailing };
}  // namespace

@interface ClearBrowsingBar ()

// Button that displays "Clear Browsing Data...".
@property(nonatomic, strong) UIButton* clearBrowsingDataButton;
// Button that displays "Edit".
@property(nonatomic, strong) UIButton* editButton;
// Button that displays "Delete".
@property(nonatomic, strong) UIButton* deleteButton;
// Button that displays "Cancel".
@property(nonatomic, strong) UIButton* cancelButton;
// Stack view for arranging the buttons.
@property(nonatomic, strong) UIStackView* stackView;

// Styles button for Leading or Trailing placement. Leading buttons have red
// text that is aligned to the leading edge. Trailing buttons have blue text
// that is aligned to the trailing edge.
- (void)styleButton:(UIButton*)button forPlacement:(ButtonPlacement)placement;

@end

@implementation ClearBrowsingBar

@synthesize editing = _editing;
@synthesize clearBrowsingDataButton = _clearBrowsingDataButton;
@synthesize editButton = _editButton;
@synthesize deleteButton = _deleteButton;
@synthesize cancelButton = _cancelButton;
@synthesize stackView = _stackView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _clearBrowsingDataButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_clearBrowsingDataButton
        setTitle:l10n_util::GetNSStringWithFixup(
                     IDS_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG)
        forState:UIControlStateNormal];
    [self styleButton:_clearBrowsingDataButton forPlacement:Leading];

    _editButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_editButton
        setTitle:l10n_util::GetNSString(IDS_HISTORY_START_EDITING_BUTTON)
        forState:UIControlStateNormal];
    [self styleButton:_editButton forPlacement:Trailing];

    _deleteButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_deleteButton setTitle:l10n_util::GetNSString(
                                IDS_HISTORY_DELETE_SELECTED_ENTRIES_BUTTON)
                   forState:UIControlStateNormal];
    [self styleButton:_deleteButton forPlacement:Leading];

    _cancelButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_cancelButton
        setTitle:l10n_util::GetNSString(IDS_HISTORY_CANCEL_EDITING_BUTTON)
        forState:UIControlStateNormal];
    [self styleButton:_cancelButton forPlacement:Trailing];

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _clearBrowsingDataButton, _editButton, _deleteButton, _cancelButton
    ]];
    _stackView.alignment = UIStackViewAlignmentFill;
    _stackView.distribution = UIStackViewDistributionEqualSpacing;
    _stackView.axis = UILayoutConstraintAxisHorizontal;

    [self addSubview:_stackView];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.layoutMarginsRelativeArrangement = YES;
    [NSLayoutConstraint activateConstraints:@[
      [_stackView.layoutMarginsGuide.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kHorizontalMargin],
      [_stackView.layoutMarginsGuide.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kHorizontalMargin],
      [_stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    [self setBackgroundColor:[UIColor whiteColor]];
    [[self layer] setShadowOpacity:kShadowOpacity];
    [self setEditing:NO];
  }
  return self;
}

#pragma mark Public Methods

- (void)setEditing:(BOOL)editing {
  _editing = editing;
  self.clearBrowsingDataButton.hidden = editing;
  self.editButton.hidden = editing;
  self.deleteButton.hidden = !editing;
  self.cancelButton.hidden = !editing;
}

- (BOOL)isEditButtonEnabled {
  return self.editButton.enabled;
}

- (void)setEditButtonEnabled:(BOOL)editButtonEnabled {
  self.editButton.enabled = editButtonEnabled;
}

- (BOOL)isDeleteButtonEnabled {
  return self.deleteButton.enabled;
}

- (void)setDeleteButtonEnabled:(BOOL)deleteButtonEnabled {
  self.deleteButton.enabled = deleteButtonEnabled;
}

- (void)setClearBrowsingDataTarget:(id)target action:(SEL)action {
  [self.clearBrowsingDataButton addTarget:target
                                   action:action
                         forControlEvents:UIControlEventTouchUpInside];
}

- (void)setEditTarget:(id)target action:(SEL)action {
  [self.editButton addTarget:target
                      action:action
            forControlEvents:UIControlEventTouchUpInside];
}

- (void)setDeleteTarget:(id)target action:(SEL)action {
  [self.deleteButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
}

- (void)setCancelTarget:(id)target action:(SEL)action {
  [self.cancelButton addTarget:target
                        action:action
              forControlEvents:UIControlEventTouchUpInside];
}

#pragma mark Private Methods

- (void)styleButton:(UIButton*)button forPlacement:(ButtonPlacement)placement {
  BOOL leading = placement == Leading;
  [button setBackgroundColor:[UIColor whiteColor]];
  UIColor* textColor = leading ? [[MDCPalette redPalette] tint500]
                               : [[MDCPalette bluePalette] tint500];
  [button setTitleColor:textColor forState:UIControlStateNormal];
  [button setTitleColor:[[MDCPalette greyPalette] tint500]
               forState:UIControlStateDisabled];
  [[button titleLabel]
      setFont:[[MDCTypography fontLoader] regularFontOfSize:14]];
  button.contentHorizontalAlignment =
      leading ^ UseRTLLayout() ? UIControlContentHorizontalAlignmentLeft
                               : UIControlContentHorizontalAlignmentRight;
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
}

@end
