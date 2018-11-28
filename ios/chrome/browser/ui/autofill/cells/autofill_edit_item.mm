// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimum gap between the label and the text field.
const CGFloat kLabelAndFieldGap = 5;
}  // namespace

@implementation AutofillEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [AutofillEditCell class];
    _returnKeyType = UIReturnKeyNext;
    _keyboardType = UIKeyboardTypeDefault;
    _autoCapitalizationType = UITextAutocapitalizationTypeWords;
  }
  return self;
}

#pragma mark TableViewItem

- (void)configureCell:(AutofillEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  NSString* textLabelFormat = self.required ? @"%@*" : @"%@";
  cell.textLabel.text =
      [NSString stringWithFormat:textLabelFormat, self.textFieldName];
  cell.textField.text = self.textFieldValue;
  if (self.textFieldName.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.textFieldName];
  }
  if (styler.cellBackgroundColor) {
    cell.textLabel.backgroundColor = styler.cellBackgroundColor;
    cell.textField.backgroundColor = styler.cellBackgroundColor;
  } else {
    cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
    cell.textField.backgroundColor = styler.tableViewBackgroundColor;
  }
  cell.textField.enabled = self.textFieldEnabled;
  cell.textField.textColor =
      self.textFieldEnabled
          ? UIColorFromRGB(kTableViewTextLabelColorBlue)
          : UIColorFromRGB(kTableViewSecondaryLabelLightGrayTextColor);
  [cell.textField addTarget:self
                     action:@selector(textFieldChanged:)
           forControlEvents:UIControlEventEditingChanged];
  cell.textField.inputView = self.inputView;
  cell.textField.returnKeyType = self.returnKeyType;
  cell.textField.keyboardType = self.keyboardType;
  cell.textField.autocapitalizationType = self.autoCapitalizationType;
  [cell setIdentifyingIcon:self.identifyingIcon];
}

#pragma mark Actions

- (void)textFieldChanged:(UITextField*)textField {
  self.textFieldValue = textField.text;
}

@end

#pragma mark - AutofillEditCell

@interface AutofillEditCell ()

@property(nonatomic, strong) NSLayoutConstraint* iconHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* iconWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textFieldTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textLabelTrailingConstraint;

// When they are activated, the label and the text field are on one line.
// They conflict with the |accessibilityConstraints|.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* standardConstraints;
// When they are activated, the label is on one line, the text field is on
// another line. They conflict with the |standardConstraints|.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* accessibilityConstraints;

// UIImageView containing the icon identifying |textField| or its current value.
@property(nonatomic, readonly, strong) UIImageView* identifyingIconView;

@end

@implementation AutofillEditCell

@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_textLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                  forAxis:UILayoutConstraintAxisHorizontal];
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    [contentView addSubview:_textLabel];

    _textField = [[UITextField alloc] init];
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textField.adjustsFontForContentSizeCategory = YES;
    [_textField
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textField];

    _textField.autocorrectionType = UITextAutocorrectionTypeNo;
    _textField.clearButtonMode = UITextFieldViewModeWhileEditing;
    _textField.contentVerticalAlignment =
        UIControlContentVerticalAlignmentCenter;

    // Card type icon.
    _identifyingIconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _identifyingIconView.translatesAutoresizingMaskIntoConstraints = NO;
    [contentView addSubview:_identifyingIconView];

    // Set up the icons size constraints. They are activated here and updated in
    // layoutSubviews.
    _iconHeightConstraint =
        [_identifyingIconView.heightAnchor constraintEqualToConstant:0];
    _iconWidthConstraint =
        [_identifyingIconView.widthAnchor constraintEqualToConstant:0];

    _textFieldTrailingConstraint = [_textField.trailingAnchor
        constraintEqualToAnchor:_identifyingIconView.leadingAnchor];
    _textLabelTrailingConstraint = [_textLabel.trailingAnchor
        constraintEqualToAnchor:_identifyingIconView.leadingAnchor];

    _standardConstraints = @[
      [_textField.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
      [_textField.leadingAnchor
          constraintEqualToAnchor:_textLabel.trailingAnchor
                         constant:kLabelAndFieldGap],
    ];

    _accessibilityConstraints = @[
      [_textField.topAnchor constraintEqualToAnchor:_textLabel.bottomAnchor
                                           constant:kTableViewVerticalSpacing],
      [_textField.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      _textLabelTrailingConstraint,
    ];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      _textFieldTrailingConstraint,
      [_identifyingIconView.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_identifyingIconView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      _iconHeightConstraint,
      _iconWidthConstraint,
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel,
                               kTableViewLargeVerticalSpacing);
    AddOptionalVerticalPadding(contentView, _textField,
                               kTableViewLargeVerticalSpacing);

    [self updateForAccessibilityContentSizeCategory:
              UIContentSizeCategoryIsAccessibilityCategory(
                  self.traitCollection.preferredContentSizeCategory)];
  }
  return self;
}

#pragma mark Public

- (void)setIdentifyingIcon:(UIImage*)icon {
  self.identifyingIconView.image = icon;
  if (icon) {
    self.textFieldTrailingConstraint.constant = -kLabelAndFieldGap;
    self.textLabelTrailingConstraint.constant = -kLabelAndFieldGap;

    // Set the size constraints of the icon view to the dimensions of the image.
    self.iconHeightConstraint.constant = icon.size.height;
    self.iconWidthConstraint.constant = icon.size.width;
  } else {
    self.textFieldTrailingConstraint.constant = 0;
    self.textLabelTrailingConstraint.constant = 0;
    self.iconHeightConstraint.constant = 0;
    self.iconWidthConstraint.constant = 0;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  BOOL isCurrentCategoryAccessibility =
      UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory);
  if (isCurrentCategoryAccessibility !=
      UIContentSizeCategoryIsAccessibilityCategory(
          previousTraitCollection.preferredContentSizeCategory)) {
    [self updateForAccessibilityContentSizeCategory:
              isCurrentCategoryAccessibility];
  }
}

#pragma mark UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textField.text = nil;
  self.textField.returnKeyType = UIReturnKeyNext;
  self.textField.keyboardType = UIKeyboardTypeDefault;
  self.textField.autocapitalizationType = UITextAutocapitalizationTypeWords;
  self.textField.autocorrectionType = UITextAutocorrectionTypeNo;
  self.textField.clearButtonMode = UITextFieldViewModeWhileEditing;
  self.textField.accessibilityIdentifier = nil;
  self.textField.enabled = NO;
  self.textField.delegate = nil;
  [self.textField removeTarget:nil
                        action:nil
              forControlEvents:UIControlEventAllEvents];
  self.identifyingIconView.image = nil;
}

#pragma mark Accessibility

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", self.textLabel.text, self.textField.text];
}

#pragma mark Private

// Updates the cell such as it is layouted correctly with regard to the
// preferred content size category, if it is an
// |accessibilityContentSizeCategory| or not.
- (void)updateForAccessibilityContentSizeCategory:
    (BOOL)accessibilityContentSizeCategory {
  if (accessibilityContentSizeCategory) {
    [NSLayoutConstraint deactivateConstraints:_standardConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentRight : NSTextAlignmentLeft;
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_standardConstraints];
    _textField.textAlignment =
        UseRTLLayout() ? NSTextAlignmentLeft : NSTextAlignmentRight;
  }
}

@end
