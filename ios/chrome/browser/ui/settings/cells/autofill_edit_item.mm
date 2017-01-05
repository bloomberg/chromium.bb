// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/autofill_edit_item.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;

// Minimum gap between the label and the text field.
const CGFloat kLabelAndFieldGap = 5;
}  // namespace

@implementation AutofillEditItem

@synthesize textFieldName = _textFieldName;
@synthesize textFieldValue = _textFieldValue;
@synthesize textFieldEnabled = _textFieldEnabled;
@synthesize autofillType = _autofillType;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillEditCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(AutofillEditCell*)cell {
  [super configureCell:cell];
  cell.textLabel.text = self.textFieldName;
  cell.textField.text = self.textFieldValue;
  if (self.textFieldName.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.textFieldName];
  }
  cell.textField.enabled = self.textFieldEnabled;
  cell.textField.textColor = self.textFieldEnabled
                                 ? [[MDCPalette cr_bluePalette] tint500]
                                 : [[MDCPalette greyPalette] tint500];
  [cell.textField addTarget:self
                     action:@selector(textFieldChanged:)
           forControlEvents:UIControlEventEditingChanged];
}

#pragma mark - Actions

- (void)textFieldChanged:(UITextField*)textField {
  self.textFieldValue = textField.text;
}

@end

@implementation AutofillEditCell

@synthesize textField = _textField;
@synthesize textLabel = _textLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    self.allowsCellInteractionsWhileEditing = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textLabel];

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_textField];

    _textLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:14];
    _textLabel.textColor = [[MDCPalette greyPalette] tint900];

    _textField.font = [[MDFRobotoFontLoader sharedInstance] lightFontOfSize:16];
    _textField.textColor = [[MDCPalette greyPalette] tint500];
    _textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.returnKeyType = UIReturnKeyDone;
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                           constant:kVerticalPadding],
      [_textLabel.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                              constant:-kVerticalPadding],
      [_textField.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_textField.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_textField.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kLabelAndFieldGap],
    ]];
    [_textField setContentHuggingPriority:UILayoutPriorityDefaultLow
                                  forAxis:UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textField.text = nil;
  self.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
  self.textField.autocorrectionType = UITextAutocorrectionTypeNo;
  self.textField.returnKeyType = UIReturnKeyDone;
  self.textField.accessibilityIdentifier = nil;
  self.textField.enabled = NO;
  self.textField.delegate = nil;
  [self.textField removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", self.textLabel.text, self.textField.text];
}

@end
