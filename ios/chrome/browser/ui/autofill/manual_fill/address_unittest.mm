// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/address.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ManualFillAddressiOSTest = PlatformTest;

// Tests that a credential is correctly created.
TEST_F(ManualFillAddressiOSTest, Creation) {
  NSString* firstName = @"First";
  NSString* middleNameOrInitial = @"M";
  NSString* lastName = @"Last";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"Country";
  ManualFillAddress* address =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_TRUE(address);
  EXPECT_TRUE([firstName isEqualToString:address.firstName]);
  EXPECT_TRUE(
      [middleNameOrInitial isEqualToString:address.middleNameOrInitial]);
  EXPECT_TRUE([lastName isEqualToString:address.lastName]);
  EXPECT_TRUE([line1 isEqualToString:address.line1]);
  EXPECT_TRUE([line2 isEqualToString:address.line2]);
  EXPECT_TRUE([zip isEqualToString:address.zip]);
  EXPECT_TRUE([city isEqualToString:address.city]);
  EXPECT_TRUE([state isEqualToString:address.state]);
  EXPECT_TRUE([country isEqualToString:address.country]);
}

// Test equality between addresses (lexicographically).
TEST_F(ManualFillAddressiOSTest, Equality) {
  NSString* firstName = @"First";
  NSString* middleNameOrInitial = @"M";
  NSString* lastName = @"Last";
  NSString* line1 = @"10 Main Street";
  NSString* line2 = @"Appt 16";
  NSString* zip = @"12345";
  NSString* city = @"Springfield";
  NSString* state = @"State";
  NSString* country = @"Country";
  ManualFillAddress* address =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];

  ManualFillAddress* equalAddress =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_TRUE([address isEqual:equalAddress]);

  ManualFillAddress* differentAddressFirstName =
      [[ManualFillAddress alloc] initWithFirstName:@"Bilbo"
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressFirstName]);

  ManualFillAddress* differentAddressMiddleNameOrInitial =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:@"R"
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressMiddleNameOrInitial]);

  ManualFillAddress* differentAddressLastName =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:@"Hobbit"
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressLastName]);

  ManualFillAddress* differentAddressLine1 =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:@"A House"
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressLine1]);

  ManualFillAddress* differentAddressLine2 =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:@""
                                               zip:zip
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressLine2]);

  ManualFillAddress* differentAddressZip =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:@"1937"
                                              city:city
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressZip]);

  ManualFillAddress* differentAddressCity =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:@"Shire"
                                             state:state
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressCity]);

  ManualFillAddress* differentAddressState =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:@"Eriador"
                                           country:country];
  EXPECT_FALSE([address isEqual:differentAddressState]);

  ManualFillAddress* differentAddressCountry =
      [[ManualFillAddress alloc] initWithFirstName:firstName
                               middleNameOrInitial:middleNameOrInitial
                                          lastName:lastName
                                             line1:line1
                                             line2:line2
                                               zip:zip
                                              city:city
                                             state:state
                                           country:@"Arnor"];
  EXPECT_FALSE([address isEqual:differentAddressCountry]);
}
