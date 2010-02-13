// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_address_model_mac.h"
#include "app/l10n_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "grit/generated_resources.h"

@implementation AutoFillAddressModel

@dynamic summary;
@synthesize label = label_;
@synthesize firstName = firstName_;
@synthesize middleName = middleName_;
@synthesize lastName = lastName_;
@synthesize email = email_;
@synthesize companyName = companyName_;
@synthesize addressLine1 = addressLine1_;
@synthesize addressLine2 = addressLine2_;
@synthesize city = city_;
@synthesize state = state_;
@synthesize zip = zip_;
@synthesize country = country_;
@synthesize phoneCountryCode = phoneCountryCode_;
@synthesize phoneAreaCode = phoneAreaCode_;
@synthesize phoneNumber = phoneNumber_;
@synthesize faxCountryCode = faxCountryCode_;
@synthesize faxAreaCode = faxAreaCode_;
@synthesize faxNumber = faxNumber_;

// Sets up the KVO dependency between "summary" and dependent fields.
+ (NSSet*)keyPathsForValuesAffectingValueForKey:(NSString*)key {
  NSSet* keyPaths = [super keyPathsForValuesAffectingValueForKey:key];

  if ([key isEqualToString:@"summary"]) {
    NSSet* affectingKeys =
        [NSSet setWithObjects:@"firstName", @"lastName", @"addressLine1", nil];
    keyPaths = [keyPaths setByAddingObjectsFromSet:affectingKeys];
  }
  return keyPaths;
}

- (id)initWithProfile:(const AutoFillProfile&)profile {
  if ((self = [super init])) {
    [self setLabel:SysUTF16ToNSString(profile.Label())];
    [self setFirstName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_FIRST)))];
    [self setMiddleName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_MIDDLE)))];
    [self setLastName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(NAME_LAST)))];
    [self setEmail:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(EMAIL_ADDRESS)))];
    [self setCompanyName:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(COMPANY_NAME)))];
    [self setAddressLine1:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)))];
    [self setAddressLine2:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)))];
    [self setCity:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_CITY)))];
    [self setState:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_STATE)))];
    [self setZip:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)))];
    [self setCountry:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)))];
    [self setPhoneCountryCode:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_HOME_COUNTRY_CODE)))];
    [self setPhoneAreaCode:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_HOME_CITY_CODE)))];
    [self setPhoneNumber:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_HOME_NUMBER)))];
    [self setFaxCountryCode:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_FAX_COUNTRY_CODE)))];
    [self setFaxAreaCode:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_FAX_CITY_CODE)))];
    [self setFaxNumber:SysUTF16ToNSString(
        profile.GetFieldText(AutoFillType(PHONE_FAX_NUMBER)))];
  }
  return self;
}

- (void)dealloc {
  [label_ release];
  [firstName_ release];
  [middleName_ release];
  [lastName_ release];
  [email_ release];
  [companyName_ release];
  [addressLine1_ release];
  [addressLine2_ release];
  [city_ release];
  [state_ release];
  [zip_ release];
  [country_ release];
  [phoneCountryCode_ release];
  [phoneAreaCode_ release];
  [phoneNumber_ release];
  [faxCountryCode_ release];
  [faxAreaCode_ release];
  [faxNumber_ release];
  [super dealloc];
}

- (NSString*)summary {
  // Create a temporary |profile| to generate summary string.
  AutoFillProfile profile(string16(), 0);
  [self copyModelToProfile:&profile];
  return SysUTF16ToNSString(profile.PreviewSummary());
}

- (void)copyModelToProfile:(AutoFillProfile*)profile {
  DCHECK(profile);
  profile->set_label(base::SysNSStringToUTF16([self label]));
  profile->SetInfo(AutoFillType(NAME_FIRST),
      base::SysNSStringToUTF16([self firstName]));
  profile->SetInfo(AutoFillType(NAME_MIDDLE),
      base::SysNSStringToUTF16([self middleName]));
  profile->SetInfo(AutoFillType(NAME_LAST),
      base::SysNSStringToUTF16([self lastName]));
  profile->SetInfo(AutoFillType(EMAIL_ADDRESS),
      base::SysNSStringToUTF16([self email]));
  profile->SetInfo(AutoFillType(COMPANY_NAME),
      base::SysNSStringToUTF16([self companyName]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE1),
      base::SysNSStringToUTF16([self addressLine1]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE2),
      base::SysNSStringToUTF16([self addressLine2]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_CITY),
      base::SysNSStringToUTF16([self city]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_STATE),
      base::SysNSStringToUTF16([self state]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_ZIP),
      base::SysNSStringToUTF16([self zip]));
  profile->SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY),
      base::SysNSStringToUTF16([self country]));
  profile->SetInfo(AutoFillType(PHONE_HOME_COUNTRY_CODE),
      base::SysNSStringToUTF16([self phoneCountryCode]));
  profile->SetInfo(AutoFillType(PHONE_HOME_CITY_CODE),
      base::SysNSStringToUTF16([self phoneAreaCode]));
  profile->SetInfo(AutoFillType(PHONE_HOME_NUMBER),
      base::SysNSStringToUTF16([self phoneNumber]));
  profile->SetInfo(AutoFillType(PHONE_FAX_COUNTRY_CODE),
      base::SysNSStringToUTF16([self faxCountryCode]));
  profile->SetInfo(AutoFillType(PHONE_FAX_CITY_CODE),
      base::SysNSStringToUTF16([self faxAreaCode]));
  profile->SetInfo(AutoFillType(PHONE_FAX_NUMBER),
      base::SysNSStringToUTF16([self faxNumber]));
}

@end
