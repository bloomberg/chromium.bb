// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that the getters and setters for country code are working.
TEST(AddressTest, CountryCode) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  address.set_country_code("US");
  EXPECT_EQ("US", address.country_code());

  address.set_country_code("CA");
  EXPECT_EQ("CA", address.country_code());
}

// Test that country codes are properly decoded as country names.
TEST(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  // Make sure that nothing breaks when the country code is missing.
  string16 country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(string16(), country);

  address.set_country_code("US");
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  address.set_country_code("CA");
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
}

// Test that we properly detect country codes appropriate for each country.
TEST(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  // Test basic conversion.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                               ASCIIToUTF16("United States"));
  string16 country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ("US", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test basic synonym detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"));
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ("US", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("canADA"));
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ("CA", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test country code detection.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("JP"));
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ("JP", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("Japan"), country);

  // Test that we ignore unknown countries.
  address.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("Unknown"));
  country = address.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(std::string(), address.country_code());
  EXPECT_EQ(string16(), country);
}

// Test that we properly match typed values to stored country data.
TEST(AddressTest, IsCountry) {
  Address address;
  address.set_country_code("US");

  const char* const kValidMatches[] = {
    "United States",
    "USA",
    "US",
    "United states",
    "us"
  };
  for (size_t i = 0; i < arraysize(kValidMatches); ++i) {
    SCOPED_TRACE(kValidMatches[i]);
    FieldTypeSet possible_field_types;
    address.GetPossibleFieldTypes(ASCIIToUTF16(kValidMatches[i]),
                                  &possible_field_types);
    ASSERT_EQ(1U, possible_field_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *possible_field_types.begin());
  }

  const char* const kInvalidMatches[] = {
    "United",
    "Garbage"
  };
  for (size_t i = 0; i < arraysize(kInvalidMatches); ++i) {
    FieldTypeSet possible_field_types;
    address.GetPossibleFieldTypes(ASCIIToUTF16(kInvalidMatches[i]),
                                  &possible_field_types);
    EXPECT_EQ(0U, possible_field_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.set_country_code("");
  EXPECT_EQ(std::string(), address.country_code());
  FieldTypeSet possible_field_types;
  address.GetPossibleFieldTypes(ASCIIToUTF16("Garbage"),
                                &possible_field_types);
  EXPECT_EQ(0U, possible_field_types.size());
}
