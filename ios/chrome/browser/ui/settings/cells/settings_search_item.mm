// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_search_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Margin for the search view.
const CGFloat kHorizontalMargin = 16.0f;
const CGFloat kVerticalMargin = 2.0f;
// Horizontal total padding for search icon.
const CGFloat kHorizontalIconPadding = 20.0f;
// Icon tint white transparency.
const CGFloat kIconTintAlpha = 0.3f;
// Background white transparency.
const CGFloat kBackgroundAlpha = 0.1f;
// Input text corner radius.
const CGFloat kCornerRadius = 12.0f;
// Input field disabled alpha.
const CGFloat kDisabledAlpha = 0.6f;
}  // namespace

@interface SettingsSearchCell ()<UITextFieldDelegate>
@end

@implementation SettingsSearchItem

@synthesize delegate = _delegate;
@synthesize placeholder = _placeholder;
@synthesize enabled = _enabled;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsSearchCell class];
    self.accessibilityTraits |= UIAccessibilityTraitSearchField;
    self.enabled = YES;
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(MDCCollectionViewCell*)cell {
  SettingsSearchCell* searchCell =
      base::mac::ObjCCastStrict<SettingsSearchCell>(cell);
  [super configureCell:searchCell];
  searchCell.textField.placeholder = self.placeholder;
  searchCell.textField.enabled = self.isEnabled;
  searchCell.textField.alpha = self.isEnabled ? 1.0f : kDisabledAlpha;
  searchCell.delegate = self.delegate;
}

@end

@implementation SettingsSearchCell

@synthesize delegate = _delegate;
@synthesize textField = _textField;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;
    contentView.clipsToBounds = YES;
    contentView.backgroundColor = [UIColor clearColor];

    UIImage* searchIcon = [[ChromeIcon searchIcon]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* searchIconView =
        [[UIImageView alloc] initWithImage:searchIcon];
    CGFloat iconFrameWidth = kHorizontalIconPadding + searchIcon.size.width;
    searchIconView.tintColor = [UIColor colorWithWhite:0 alpha:kIconTintAlpha];
    searchIconView.frame =
        CGRectMake(0.0, 0.0, iconFrameWidth, searchIcon.size.height);
    searchIconView.contentMode = UIViewContentModeCenter;

    _textField = [[UITextField alloc] init];
    _textField.accessibilityIdentifier = @"SettingsSearchCellTextField";
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;
    _textField.backgroundColor =
        [UIColor colorWithWhite:0 alpha:kBackgroundAlpha];
    _textField.textColor =
        [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
    _textField.font = [MDCTypography subheadFont];
    [[_textField layer] setCornerRadius:kCornerRadius];
    [_textField setLeftViewMode:UITextFieldViewModeAlways];
    _textField.leftView = searchIconView;
    _textField.clearButtonMode = UITextFieldViewModeAlways;
    [_textField setRightViewMode:UITextFieldViewModeNever];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    _textField.spellCheckingType = UITextSpellCheckingTypeNo;
    [_textField setDelegate:self];

    [contentView addSubview:_textField];

    AddSameConstraintsToSidesWithInsets(
        _textField, contentView,
        LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom |
            LayoutSides::kTop,
        ChromeDirectionalEdgeInsetsMake(kVerticalMargin, kHorizontalMargin,
                                        kVerticalMargin, kHorizontalMargin));
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textField.placeholder = @"";
  self.textField.enabled = YES;
  self.textField.alpha = 1.0f;
  self.delegate = nil;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  NSMutableString* text = [NSMutableString stringWithString:[textField text]];
  [text replaceCharactersInRange:range withString:string];
  [self.delegate didRequestSearchForTerm:text];
  return YES;
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  [self.delegate didRequestSearchForTerm:@""];
  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [self.textField resignFirstResponder];
  return YES;
}

@end
