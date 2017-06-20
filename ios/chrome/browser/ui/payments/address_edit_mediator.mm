// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/address_edit_mediator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_combobox_model.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "ios/chrome/grit/ios_strings.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AddressEditMediator () {
  std::unique_ptr<RegionDataLoader> _regionDataLoader;
}

// The PaymentRequest object owning an instance of web::PaymentRequest as
// provided by the page invoking the Payment Request API. This is a weak
// pointer and should outlive this class.
@property(nonatomic, assign) PaymentRequest* paymentRequest;

// The address to be edited, if any. This pointer is not owned by this class and
// should outlive it.
@property(nonatomic, assign) autofill::AutofillProfile* address;

// The map of autofill types to the cached editor fields. Helps reuse the editor
// fields and therefore maintain their existing values when the selected country
// changes and the editor fields get updated.
@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, EditorField*>* fieldsMap;

// The list of current editor fields.
@property(nonatomic, strong) NSMutableArray<EditorField*>* fields;

// The reference to the autofill::ADDRESS_HOME_STATE field, if any.
@property(nonatomic, strong) EditorField* regionField;

@end

@implementation AddressEditMediator

@synthesize state = _state;
@synthesize consumer = _consumer;
@synthesize countries = _countries;
@synthesize selectedCountryCode = _selectedCountryCode;
@synthesize paymentRequest = _paymentRequest;
@synthesize address = _address;
@synthesize fieldsMap = _fieldsMap;
@synthesize fields = _fields;
@synthesize regionField = _regionField;

- (instancetype)initWithPaymentRequest:(PaymentRequest*)paymentRequest
                               address:(autofill::AutofillProfile*)address {
  self = [super init];
  if (self) {
    _paymentRequest = paymentRequest;
    _address = address;
    _state =
        _address ? EditViewControllerStateEdit : EditViewControllerStateCreate;
    _fieldsMap = [[NSMutableDictionary alloc] init];
    [self loadCountries];
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<PaymentRequestEditConsumer>)consumer {
  _consumer = consumer;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.regionField)
    [self loadRegions];
}

- (void)setSelectedCountryCode:(NSString*)selectedCountryCode {
  if (_selectedCountryCode == selectedCountryCode)
    return;
  _selectedCountryCode = selectedCountryCode;

  [self.consumer setEditorFields:[self createEditorFields]];
  if (self.regionField)
    [self loadRegions];
}

#pragma mark - CreditCardEditViewControllerDataSource

- (CollectionViewItem*)headerItem {
  return nil;
}

- (BOOL)shouldHideBackgroundForHeaderItem {
  return NO;
}

- (UIImage*)iconIdentifyingEditorField:(EditorField*)field {
  return nil;
}

#pragma mark - RegionDataLoaderConsumer

- (void)regionDataLoaderDidSucceedWithRegions:
    (NSDictionary<NSString*, NSString*>*)regions {
  // Enable the previously disabled field.
  self.regionField.enabled = YES;

  // An autofill profile may have a region code or a region name stored as the
  // autofill::ADDRESS_HOME_STATE. If an address is being edited whose value for
  // that field type is a valid region code or a valid region name, the editor
  // field value is set to the respective region code. Otherwise, it is set to
  // nil.
  self.regionField.value = nil;
  if (self.address) {
    NSString* region =
        [self fieldValueFromProfile:self.address
                          fieldType:autofill::ADDRESS_HOME_STATE];
    if ([regions objectForKey:region]) {
      self.regionField.value = region;
    } else if ([[regions allKeysForObject:region] count]) {
      DCHECK(1 == [[regions allKeysForObject:region] count]);
      self.regionField.value = [regions allKeysForObject:region][0];
    }
  }

  // Notify the view controller asynchronously to allow for the view to update.
  __weak AddressEditMediator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.consumer setOptions:[regions allKeys]
                   forEditorField:weakSelf.regionField];
  });
}

#pragma mark - Helper methods

// Loads the country codes and names and sets the default selected country code.
- (void)loadCountries {
  autofill::CountryComboboxModel countryModel;
  countryModel.SetCountries(*_paymentRequest->GetPersonalDataManager(),
                            base::Callback<bool(const std::string&)>(),
                            GetApplicationContext()->GetApplicationLocale());
  const autofill::CountryComboboxModel::CountryVector& countriesVector =
      countryModel.countries();

  NSMutableDictionary<NSString*, NSString*>* countries =
      [[NSMutableDictionary alloc]
          initWithCapacity:static_cast<NSUInteger>(countriesVector.size())];
  for (size_t i = 0; i < countriesVector.size(); ++i) {
    if (countriesVector[i].get()) {
      [countries setObject:base::SysUTF16ToNSString(countriesVector[i]->name())
                    forKey:base::SysUTF8ToNSString(
                               countriesVector[i]->country_code())];
    }
  }
  _countries = countries;

  // If an address is being edited and it has a valid country code, the selected
  // country code is set to that value. Otherwise, it is set to the default
  // country code.
  NSString* countryCode =
      [self fieldValueFromProfile:_address
                        fieldType:autofill::ADDRESS_HOME_COUNTRY];
  _selectedCountryCode =
      countryCode && [_countries objectForKey:countryCode]
          ? countryCode
          : base::SysUTF8ToNSString(countryModel.GetDefaultCountryCode());
}

// Queries the region names based on the selected country code.
- (void)loadRegions {
  _regionDataLoader = base::MakeUnique<RegionDataLoader>(self);
  _regionDataLoader->LoadRegionData(
      base::SysNSStringToUTF8(self.selectedCountryCode),
      _paymentRequest->GetRegionDataLoader());
}

// Returns an array of editor fields based on the selected country code. Caches
// the fields to be reused when the selected country code changes.
- (NSArray<EditorField*>*)createEditorFields {
  self.fields = [[NSMutableArray alloc] init];

  self.regionField = nil;

  base::ListValue addressComponents;
  std::string unused;
  autofill::GetAddressComponents(
      base::SysNSStringToUTF8(self.selectedCountryCode),
      GetApplicationContext()->GetApplicationLocale(), &addressComponents,
      &unused);

  for (size_t lineIndex = 0; lineIndex < addressComponents.GetSize();
       ++lineIndex) {
    const base::ListValue* line = nullptr;
    if (!addressComponents.GetList(lineIndex, &line)) {
      NOTREACHED();
      return @[];
    }
    for (size_t componentIndex = 0; componentIndex < line->GetSize();
         ++componentIndex) {
      const base::DictionaryValue* component = nullptr;
      if (!line->GetDictionary(componentIndex, &component)) {
        NOTREACHED();
        return @[];
      }

      std::string autofillType;
      if (!component->GetString(autofill::kFieldTypeKey, &autofillType)) {
        NOTREACHED();
        return @[];
      }
      AutofillUIType autofillUIType = AutofillUITypeFromAutofillType(
          autofill::GetFieldTypeFromString(autofillType));

      NSNumber* fieldKey = [NSNumber numberWithInt:autofillUIType];
      EditorField* field = self.fieldsMap[fieldKey];
      if (!field) {
        NSString* value =
            [self fieldValueFromProfile:self.address
                              fieldType:autofill::GetFieldTypeFromString(
                                            autofillType)];
        BOOL required = autofillUIType != AutofillUITypeProfileCompanyName;
        field =
            [[EditorField alloc] initWithAutofillUIType:autofillUIType
                                              fieldType:EditorFieldTypeTextField
                                                  label:nil
                                                  value:value
                                               required:required];
        [self.fieldsMap setObject:field forKey:fieldKey];
      }

      std::string fieldLabel;
      if (!component->GetString(autofill::kFieldNameKey, &fieldLabel)) {
        NOTREACHED();
        return @[];
      }
      field.label = base::SysUTF8ToNSString(fieldLabel);

      // Keep a reference to the field for the autofill::ADDRESS_HOME_STATE. Set
      // its value to "Loading..." and disable it until the regions are loaded.
      if (autofillUIType == AutofillUITypeProfileHomeAddressState) {
        self.regionField = field;
        field.value = l10n_util::GetNSString(IDS_AUTOFILL_LOADING_REGIONS);
        field.enabled = NO;
      }

      [self.fields addObject:field];

      // Insert the country field right after the full name field.
      if (autofillUIType == AutofillUITypeProfileFullName) {
        NSNumber* countryFieldKey =
            [NSNumber numberWithInt:AutofillUITypeProfileHomeAddressCountry];
        EditorField* field = self.fieldsMap[countryFieldKey];
        if (!field) {
          NSString* label = l10n_util::GetNSString(
              IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL);
          field = [[EditorField alloc]
              initWithAutofillUIType:AutofillUITypeProfileHomeAddressCountry
                           fieldType:EditorFieldTypeSelector
                               label:label
                               value:nil
                            required:YES];
          [self.fieldsMap setObject:field forKey:countryFieldKey];
        }
        field.value = self.selectedCountryCode;
        field.displayValue = self.countries[self.selectedCountryCode];
        [self.fields addObject:field];
      }
    }
  }

  // Always add phone number field at the end.
  NSNumber* phoneNumberFieldKey =
      [NSNumber numberWithInt:AutofillUITypeProfileHomePhoneWholeNumber];
  EditorField* field = self.fieldsMap[phoneNumberFieldKey];
  if (!field) {
    NSString* value =
        [self fieldValueFromProfile:self.address
                          fieldType:autofill::PHONE_HOME_WHOLE_NUMBER];
    field = [[EditorField alloc]
        initWithAutofillUIType:AutofillUITypeProfileHomePhoneWholeNumber
                     fieldType:EditorFieldTypeTextField
                         label:l10n_util::GetNSString(IDS_IOS_AUTOFILL_PHONE)
                         value:value
                      required:YES];
    [self.fieldsMap setObject:field forKey:phoneNumberFieldKey];
  }
  [self.fields addObject:field];

  return self.fields;
}

// Takes in an autofill profile and an autofill field type and returns the
// corresponding field value. Returns nil if |profile| is nullptr.
- (NSString*)fieldValueFromProfile:(autofill::AutofillProfile*)profile
                         fieldType:(autofill::ServerFieldType)fieldType {
  return profile ? base::SysUTF16ToNSString(profile->GetInfo(
                       autofill::AutofillType(fieldType),
                       GetApplicationContext()->GetApplicationLocale()))
                 : nil;
}

@end
