// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_cell.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillAddressItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentDelegate> delegate;

// The address/profile for this item.
@property(nonatomic, readonly) autofill::AutofillProfile profile;

@end

@implementation ManualFillAddressItem

- (instancetype)
initWithAutofillProfile:(const autofill::AutofillProfile&)profile
               delegate:(id<ManualFillContentDelegate>)delegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _delegate = delegate;
    _profile = profile;
    self.cellClass = [ManualFillAddressCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillAddressCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithAutofillProfile:self.profile delegate:self.delegate];
}

@end

namespace {

// Left and right margins of the cell content.
static const CGFloat sideMargins = 16;

// Margin left and right of multiple buttons on same line.
static const CGFloat InnerMarginWidth = 16.0;

}  // namespace

@interface ManualFillAddressCell ()

// The separator gray line.
@property(nonatomic, strong) UIView* grayLine;

// The label with the line1 -- line2.
@property(nonatomic, strong) UILabel* addressLabel;

// The vertical constraints for all the lines.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* verticalConstraints;

// The constraints for the first/middle/last name line.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* nameLineConstraints;

// The constraints for the zip/city line.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* zipCityLineConstraints;

// The constraints for the state/country line.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* stateCountryLineConstraints;

// A button showing the address associated first name.
@property(nonatomic, strong) UIButton* firstNameButton;

// The separator label between first and middle name/initial.
@property(nonatomic, strong) UILabel* middleNameSeparatorLabel;

// A button showing the address associated middle name or initial.
@property(nonatomic, strong) UIButton* middleNameButton;

// The separator label between middle name/initial and last name.
@property(nonatomic, strong) UILabel* lastNameSeparatorLabel;

// A button showing the address associated last name.
@property(nonatomic, strong) UIButton* lastNameButton;

// A button showing the address line 1.
@property(nonatomic, strong) UIButton* line1Button;

// An optional button showing the address line 2.
@property(nonatomic, strong) UIButton* line2Button;

// A button showing zip code.
@property(nonatomic, strong) UIButton* zipButton;

// The separator label between zip and city.
@property(nonatomic, strong) UILabel* citySeparatorLabel;

// A button showing city.
@property(nonatomic, strong) UIButton* cityButton;

// A button showing state/province.
@property(nonatomic, strong) UIButton* stateButton;

// The separator label between state and country.
@property(nonatomic, strong) UILabel* countrySeparatorLabel;

// A button showing country.
@property(nonatomic, strong) UIButton* countryButton;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentDelegate> delegate;

// The credit address/profile data for this cell.
// TODO(crbug.com/845472): move to cocoa model.
@property(nonatomic, assign) autofill::AutofillProfile profile;

@end

@implementation ManualFillAddressCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.verticalConstraints];
  self.verticalConstraints = @[];
  [NSLayoutConstraint deactivateConstraints:self.nameLineConstraints];
  self.nameLineConstraints = @[];
  [NSLayoutConstraint deactivateConstraints:self.zipCityLineConstraints];
  self.zipCityLineConstraints = @[];
  [NSLayoutConstraint deactivateConstraints:self.stateCountryLineConstraints];
  self.stateCountryLineConstraints = @[];

  self.addressLabel.text = @"";
  [self.firstNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.middleNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.lastNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.line1Button setTitle:@"" forState:UIControlStateNormal];
  [self.line2Button setTitle:@"" forState:UIControlStateNormal];
  [self.zipButton setTitle:@"" forState:UIControlStateNormal];
  [self.cityButton setTitle:@"" forState:UIControlStateNormal];
  [self.stateButton setTitle:@"" forState:UIControlStateNormal];
  [self.countryButton setTitle:@"" forState:UIControlStateNormal];
  [self.line2Button setTitle:@"" forState:UIControlStateNormal];
  self.delegate = nil;
  // TODO(crbug.com/845472): clear profile.
}

- (void)setUpWithAutofillProfile:(const autofill::AutofillProfile&)profile
                        delegate:(id<ManualFillContentDelegate>)delegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.delegate = delegate;
  self.profile = profile;

  NSMutableArray<UIView*>* verticalLeadViews = [[NSMutableArray alloc] init];
  NSString* line1 = [self fieldValueOfType:autofill::ADDRESS_HOME_LINE1];
  NSString* line2 = [self fieldValueOfType:autofill::ADDRESS_HOME_LINE2];

  // Top label, summary of line 1 and 2.
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:line1 ? line1 : @""
              attributes:@{
                NSForegroundColorAttributeName : UIColor.blackColor,
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
  if (line2.length) {
    NSString* line2String = [NSString stringWithFormat:@" –– %@", line2];
    NSDictionary* attributes = @{
      NSForegroundColorAttributeName : UIColor.lightGrayColor,
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
    };
    NSAttributedString* line2StringAttributedString =
        [[NSAttributedString alloc] initWithString:line2String
                                        attributes:attributes];
    [attributedString appendAttributedString:line2StringAttributedString];
  }
  self.addressLabel.attributedText = attributedString;
  [verticalLeadViews addObject:self.addressLabel];

  // Name line, first middle and last.
  NSMutableArray<UIView*>* nameLineViews = [[NSMutableArray alloc] init];
  NSString* firstName = [self fieldValueOfType:autofill::NAME_FIRST];
  NSString* lastName = [self fieldValueOfType:autofill::NAME_LAST];
  NSString* middleName = [self fieldValueOfType:autofill::NAME_MIDDLE];
  if (!middleName || middleName.length == 0) {
    middleName = [self fieldValueOfType:autofill::NAME_MIDDLE_INITIAL];
  }

  bool showFirstName = firstName.length;
  bool showMiddleName = middleName.length;
  bool showLastName = lastName.length;

  if (showFirstName) {
    [self.firstNameButton setTitle:firstName forState:UIControlStateNormal];
    [nameLineViews addObject:self.firstNameButton];
    self.firstNameButton.hidden = NO;
  } else {
    self.firstNameButton.hidden = YES;
  }

  if (showFirstName && showMiddleName) {
    [nameLineViews addObject:self.middleNameSeparatorLabel];
    self.middleNameSeparatorLabel.hidden = NO;
  } else {
    self.middleNameSeparatorLabel.hidden = YES;
  }

  if (showMiddleName) {
    [self.middleNameButton setTitle:middleName forState:UIControlStateNormal];
    [nameLineViews addObject:self.middleNameButton];
    self.middleNameButton.hidden = NO;
  } else {
    self.middleNameButton.hidden = YES;
  }

  if ((showFirstName || showMiddleName) && showLastName) {
    [nameLineViews addObject:self.lastNameSeparatorLabel];
    self.lastNameSeparatorLabel.hidden = NO;
  } else {
    self.lastNameSeparatorLabel.hidden = YES;
  }

  if (showLastName) {
    [self.lastNameButton setTitle:lastName forState:UIControlStateNormal];
    [nameLineViews addObject:self.lastNameButton];
    self.lastNameButton.hidden = NO;
  } else {
    self.lastNameButton.hidden = YES;
  }

  self.nameLineConstraints = HorizontalConstraintsForViewsOnGuideWithShift(
      nameLineViews, self.grayLine, -InnerMarginWidth);

  if (nameLineViews.count) {
    [verticalLeadViews addObject:nameLineViews.firstObject];
  }

  // Address line 1.
  if (line1.length) {
    [self.line1Button setTitle:line1 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line1Button];
    self.line1Button.hidden = NO;
  } else {
    self.line1Button.hidden = YES;
  }

  // Address line 2.
  if (line2.length) {
    [self.line2Button setTitle:line2 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line2Button];
    self.line2Button.hidden = NO;
  } else {
    self.line2Button.hidden = YES;
  }

  // Zip and city line.
  NSMutableArray<UIView*>* zipCityLineViews = [[NSMutableArray alloc] init];
  NSString* zip = [self fieldValueOfType:autofill::ADDRESS_HOME_ZIP];
  NSString* city = [self fieldValueOfType:autofill::ADDRESS_HOME_CITY];

  if (zip.length) {
    [self.zipButton setTitle:zip forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.zipButton];
    self.zipButton.hidden = NO;
  } else {
    self.zipButton.hidden = YES;
  }

  if (zip.length && city.length) {
    [zipCityLineViews addObject:self.citySeparatorLabel];
    self.citySeparatorLabel.hidden = NO;
  } else {
    self.citySeparatorLabel.hidden = YES;
  }

  if (city.length) {
    [self.cityButton setTitle:city forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.cityButton];
    self.cityButton.hidden = NO;
  } else {
    self.cityButton.hidden = YES;
  }

  self.zipCityLineConstraints = HorizontalConstraintsForViewsOnGuideWithShift(
      zipCityLineViews, self.grayLine, -InnerMarginWidth);
  if (zipCityLineViews.count) {
    [verticalLeadViews addObject:zipCityLineViews.firstObject];
  }

  // State and country line.
  NSMutableArray<UIView*>* stateCountryLineViews =
      [[NSMutableArray alloc] init];
  NSString* state = [self fieldValueOfType:autofill::ADDRESS_HOME_STATE];
  NSString* country = [self fieldValueOfType:autofill::ADDRESS_HOME_COUNTRY];

  if (state.length) {
    [self.stateButton setTitle:state forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.stateButton];
    self.stateButton.hidden = NO;
  } else {
    self.stateButton.hidden = YES;
  }

  if (state.length && country.length) {
    [stateCountryLineViews addObject:self.countrySeparatorLabel];
    self.countrySeparatorLabel.hidden = NO;
  } else {
    self.countrySeparatorLabel.hidden = YES;
  }

  if (country.length) {
    [self.countryButton setTitle:country forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.countryButton];
    self.countryButton.hidden = NO;
  } else {
    self.countryButton.hidden = YES;
  }

  self.stateCountryLineConstraints =
      HorizontalConstraintsForViewsOnGuideWithShift(
          stateCountryLineViews, self.grayLine, -InnerMarginWidth);
  if (stateCountryLineViews.count) {
    [verticalLeadViews addObject:stateCountryLineViews.firstObject];
  }

  self.verticalConstraints = VerticalConstraintsSpacingForViewsInContainer(
      verticalLeadViews, self.contentView);
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.grayLine = [[UIView alloc] init];
  self.grayLine.backgroundColor = [UIColor colorWithWhite:0.88 alpha:1];
  self.grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.grayLine];

  self.addressLabel = CreateLabel();
  [self.contentView addSubview:self.addressLabel];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.addressLabel ],
                                                self.grayLine, 0);

  self.firstNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.firstNameButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.firstNameButton,
                                                InnerMarginWidth);

  self.middleNameSeparatorLabel = CreateLabel();
  self.middleNameSeparatorLabel.text = @"·";
  [self.contentView addSubview:self.middleNameSeparatorLabel];

  self.middleNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.middleNameButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.middleNameButton,
                                                InnerMarginWidth);

  self.lastNameSeparatorLabel = CreateLabel();
  self.lastNameSeparatorLabel.text = @"·";
  [self.contentView addSubview:self.lastNameSeparatorLabel];

  self.lastNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.lastNameButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.lastNameButton,
                                                InnerMarginWidth);

  SyncBaselinesForViewsOnView(
      @[
        self.middleNameSeparatorLabel, self.middleNameButton,
        self.lastNameSeparatorLabel, self.lastNameButton
      ],
      self.firstNameButton);

  self.line1Button = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line1Button];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.line1Button ],
                                                self.grayLine, 0);

  self.line2Button = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line2Button];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.line2Button ],
                                                self.grayLine, 0);

  self.zipButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.zipButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.zipButton,
                                                InnerMarginWidth);

  self.citySeparatorLabel = CreateLabel();
  self.citySeparatorLabel.text = @"·";
  [self.contentView addSubview:self.citySeparatorLabel];

  self.cityButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.cityButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.cityButton,
                                                InnerMarginWidth);

  SyncBaselinesForViewsOnView(@[ self.citySeparatorLabel, self.cityButton ],
                              self.zipButton);

  self.stateButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.stateButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.stateButton,
                                                InnerMarginWidth);

  self.countrySeparatorLabel = CreateLabel();
  self.countrySeparatorLabel.text = @"·";
  [self.contentView addSubview:self.countrySeparatorLabel];

  self.countryButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.countryButton];
  HorizontalConstraintsMarginForButtonWithWidth(self.countryButton,
                                                InnerMarginWidth);

  SyncBaselinesForViewsOnView(
      @[ self.countrySeparatorLabel, self.countryButton ], self.stateButton);

  self.nameLineConstraints = @[];
  self.zipCityLineConstraints = @[];
  self.stateCountryLineConstraints = @[];
  self.verticalConstraints = @[];

  id<LayoutGuideProvider> safeArea =
      SafeAreaLayoutGuideForView(self.contentView);

  [NSLayoutConstraint activateConstraints:@[
    // Common vertical constraints.
    [self.grayLine.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.grayLine.heightAnchor constraintEqualToConstant:1],

    // Horizontal constraints.
    [self.grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                                constant:sideMargins],
    [safeArea.trailingAnchor
        constraintEqualToAnchor:self.grayLine.trailingAnchor
                       constant:sideMargins],
  ]];
}

// Takes in an autofill profile and an autofill field type and returns the
// corresponding field value.
- (NSString*)fieldValueOfType:(autofill::ServerFieldType)fieldType {
  return base::SysUTF16ToNSString(
      self.profile.GetInfo(autofill::AutofillType(fieldType),
                           GetApplicationContext()->GetApplicationLocale()));
}

- (void)userDidTapAddressInfo:(UIButton*)sender {
  [self.delegate userDidPickContent:sender.titleLabel.text isSecure:NO];
}

@end
