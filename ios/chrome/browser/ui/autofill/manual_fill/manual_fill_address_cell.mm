// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_cell.h"

#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillAddressItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentDelegate> delegate;

// The address/profile for this item.
@property(nonatomic, readonly) ManualFillAddress* address;

@end

@implementation ManualFillAddressItem

- (instancetype)initWithAddress:(ManualFillAddress*)address
                       delegate:(id<ManualFillContentDelegate>)delegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _delegate = delegate;
    _address = address;
    self.cellClass = [ManualFillAddressCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillAddressCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithAddress:self.address delegate:self.delegate];
}

@end

@interface ManualFillAddressCell ()

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

// A button showing the company name.
@property(nonatomic, strong) UIButton* companyButton;

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

// A button showing a phone number.
@property(nonatomic, strong) UIButton* phoneNumberButton;

// A button showing an email address.
@property(nonatomic, strong) UIButton* emailAddressButton;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentDelegate> delegate;

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
  [self.companyButton setTitle:@"" forState:UIControlStateNormal];
  [self.line1Button setTitle:@"" forState:UIControlStateNormal];
  [self.line2Button setTitle:@"" forState:UIControlStateNormal];
  [self.zipButton setTitle:@"" forState:UIControlStateNormal];
  [self.cityButton setTitle:@"" forState:UIControlStateNormal];
  [self.stateButton setTitle:@"" forState:UIControlStateNormal];
  [self.countryButton setTitle:@"" forState:UIControlStateNormal];
  [self.phoneNumberButton setTitle:@"" forState:UIControlStateNormal];
  [self.emailAddressButton setTitle:@"" forState:UIControlStateNormal];
  self.delegate = nil;
}

- (void)setUpWithAddress:(ManualFillAddress*)address
                delegate:(id<ManualFillContentDelegate>)delegate {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.delegate = delegate;

  NSMutableArray<UIView*>* verticalLeadViews = [[NSMutableArray alloc] init];
  UIView* guide = self.contentView;

  // Top label, summary of line 1 and 2.
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:address.line1 ? address.line1 : @""
              attributes:@{
                NSForegroundColorAttributeName : UIColor.blackColor,
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
  if (address.line2.length) {
    NSString* line2String =
        [NSString stringWithFormat:@" –– %@", address.line2];
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

  bool showFirstName = address.firstName.length;
  bool showMiddleName = address.middleNameOrInitial.length;
  bool showLastName = address.lastName.length;

  if (showFirstName) {
    [self.firstNameButton setTitle:address.firstName
                          forState:UIControlStateNormal];
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
    [self.middleNameButton setTitle:address.middleNameOrInitial
                           forState:UIControlStateNormal];
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
    [self.lastNameButton setTitle:address.lastName
                         forState:UIControlStateNormal];
    [nameLineViews addObject:self.lastNameButton];
    self.lastNameButton.hidden = NO;
  } else {
    self.lastNameButton.hidden = YES;
  }

  self.nameLineConstraints =
      HorizontalConstraintsForViewsOnGuideWithShift(nameLineViews, guide, 0);

  if (nameLineViews.count) {
    [verticalLeadViews addObject:nameLineViews.firstObject];
  }

  // Company line.
  if (address.company.length) {
    [self.companyButton setTitle:address.company forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.companyButton];
    self.companyButton.hidden = NO;
  } else {
    self.companyButton.hidden = YES;
  }

  // Address line 1.
  if (address.line1.length) {
    [self.line1Button setTitle:address.line1 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line1Button];
    self.line1Button.hidden = NO;
  } else {
    self.line1Button.hidden = YES;
  }

  // Address line 2.
  if (address.line2.length) {
    [self.line2Button setTitle:address.line2 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line2Button];
    self.line2Button.hidden = NO;
  } else {
    self.line2Button.hidden = YES;
  }

  // Zip and city line.
  NSMutableArray<UIView*>* zipCityLineViews = [[NSMutableArray alloc] init];

  if (address.zip.length) {
    [self.zipButton setTitle:address.zip forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.zipButton];
    self.zipButton.hidden = NO;
  } else {
    self.zipButton.hidden = YES;
  }

  if (address.zip.length && address.city.length) {
    [zipCityLineViews addObject:self.citySeparatorLabel];
    self.citySeparatorLabel.hidden = NO;
  } else {
    self.citySeparatorLabel.hidden = YES;
  }

  if (address.city.length) {
    [self.cityButton setTitle:address.city forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.cityButton];
    self.cityButton.hidden = NO;
  } else {
    self.cityButton.hidden = YES;
  }

  self.zipCityLineConstraints =
      HorizontalConstraintsForViewsOnGuideWithShift(zipCityLineViews, guide, 0);
  if (zipCityLineViews.count) {
    [verticalLeadViews addObject:zipCityLineViews.firstObject];
  }

  // State and country line.
  NSMutableArray<UIView*>* stateCountryLineViews =
      [[NSMutableArray alloc] init];

  if (address.state.length) {
    [self.stateButton setTitle:address.state forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.stateButton];
    self.stateButton.hidden = NO;
  } else {
    self.stateButton.hidden = YES;
  }

  if (address.state.length && address.country.length) {
    [stateCountryLineViews addObject:self.countrySeparatorLabel];
    self.countrySeparatorLabel.hidden = NO;
  } else {
    self.countrySeparatorLabel.hidden = YES;
  }

  if (address.country.length) {
    [self.countryButton setTitle:address.country forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.countryButton];
    self.countryButton.hidden = NO;
  } else {
    self.countryButton.hidden = YES;
  }

  self.stateCountryLineConstraints =
      HorizontalConstraintsForViewsOnGuideWithShift(stateCountryLineViews,
                                                    guide, 0);
  if (stateCountryLineViews.count) {
    [verticalLeadViews addObject:stateCountryLineViews.firstObject];
  }

  if (address.phoneNumber.length) {
    [self.phoneNumberButton setTitle:address.phoneNumber
                            forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.phoneNumberButton];
    self.phoneNumberButton.hidden = NO;
  } else {
    self.phoneNumberButton.hidden = YES;
  }

  if (address.emailAddress.length) {
    [self.emailAddressButton setTitle:address.emailAddress
                             forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.emailAddressButton];
    self.emailAddressButton.hidden = NO;
  } else {
    self.emailAddressButton.hidden = YES;
  }

  self.verticalConstraints = VerticalConstraintsSpacingForViewsInContainer(
      verticalLeadViews, self.contentView);
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  UIView* guide = self.contentView;
  CreateGraySeparatorForContainer(guide);

  self.addressLabel = CreateLabel();
  [self.contentView addSubview:self.addressLabel];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.addressLabel ], guide,
                                                ButtonHorizontalMargin);

  self.firstNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.firstNameButton];

  self.middleNameSeparatorLabel = CreateLabel();
  self.middleNameSeparatorLabel.text = @"·";
  [self.contentView addSubview:self.middleNameSeparatorLabel];

  self.middleNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.middleNameButton];

  self.lastNameSeparatorLabel = CreateLabel();
  self.lastNameSeparatorLabel.text = @"·";
  [self.contentView addSubview:self.lastNameSeparatorLabel];

  self.lastNameButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.lastNameButton];

  SyncBaselinesForViewsOnView(
      @[
        self.middleNameSeparatorLabel, self.middleNameButton,
        self.lastNameSeparatorLabel, self.lastNameButton
      ],
      self.firstNameButton);

  self.companyButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.companyButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.companyButton ], guide,
                                                0);

  self.line1Button = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line1Button];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.line1Button ], guide,
                                                0);

  self.line2Button = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line2Button];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.line2Button ], guide,
                                                0);

  self.zipButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.zipButton];

  self.citySeparatorLabel = CreateLabel();
  self.citySeparatorLabel.text = @"·";
  [self.contentView addSubview:self.citySeparatorLabel];

  self.cityButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.cityButton];

  SyncBaselinesForViewsOnView(@[ self.citySeparatorLabel, self.cityButton ],
                              self.zipButton);

  self.stateButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.stateButton];

  self.countrySeparatorLabel = CreateLabel();
  self.countrySeparatorLabel.text = @"·";
  [self.contentView addSubview:self.countrySeparatorLabel];

  self.countryButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.countryButton];

  SyncBaselinesForViewsOnView(
      @[ self.countrySeparatorLabel, self.countryButton ], self.stateButton);

  self.phoneNumberButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.phoneNumberButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.phoneNumberButton ],
                                                guide, 0);

  self.emailAddressButton = CreateButtonWithSelectorAndTarget(
      @selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.emailAddressButton];
  HorizontalConstraintsForViewsOnGuideWithShift(@[ self.emailAddressButton ],
                                                guide, 0);

  self.nameLineConstraints = @[];
  self.zipCityLineConstraints = @[];
  self.stateCountryLineConstraints = @[];
  self.verticalConstraints = @[];
}

- (void)userDidTapAddressInfo:(UIButton*)sender {
  const char* metricsAction = nullptr;
  if (sender == self.firstNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectFirstName";
  } else if (sender == self.middleNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectMiddleName";
  } else if (sender == self.lastNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectLastName";
  } else if (sender == self.companyButton) {
    metricsAction = "ManualFallback_Profiles_Company";
  } else if (sender == self.line1Button) {
    metricsAction = "ManualFallback_Profiles_Address1";
  } else if (sender == self.line2Button) {
    metricsAction = "ManualFallback_Profiles_Address2";
  } else if (sender == self.zipButton) {
    metricsAction = "ManualFallback_Profiles_Zip";
  } else if (sender == self.cityButton) {
    metricsAction = "ManualFallback_Profiles_City";
  } else if (sender == self.stateButton) {
    metricsAction = "ManualFallback_Profiles_State";
  } else if (sender == self.countryButton) {
    metricsAction = "ManualFallback_Profiles_Country";
  } else if (sender == self.phoneNumberButton) {
    metricsAction = "ManualFallback_Profiles_PhoneNumber";
  } else if (sender == self.emailAddressButton) {
    metricsAction = "ManualFallback_Profiles_EmailAddress";
  }
  DCHECK(metricsAction);
  base::RecordAction(base::UserMetricsAction(metricsAction));

  [self.delegate userDidPickContent:sender.titleLabel.text
                    isPasswordField:NO
                      requiresHTTPS:NO];
}

@end
