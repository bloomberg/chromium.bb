// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class AddressTest : public testing::Test {
 public:
  // In order to access the application locale -- which the tested functions do
  // internally -- this test must run on the UI thread.
  AddressTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(AddressTest);
};

// Test that the getters and setters for country code are working.
TEST_F(AddressTest, CountryCode) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  address.set_country_code("US");
  EXPECT_EQ("US", address.country_code());

  address.set_country_code("CA");
  EXPECT_EQ("CA", address.country_code());
}

// Test that country codes are properly decoded as country names.
TEST_F(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  // Make sure that nothing breaks when the country code is missing.
  string16 country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ(string16(), country);

  address.set_country_code("US");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  address.set_country_code("CA");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
}

// Test that we properly detect country codes appropriate for each country.
TEST_F(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(std::string(), address.country_code());

  // Test basic conversion.
  address.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("United States"), "en-US");
  string16 country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ("US", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test basic synonym detection.
  address.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("USA"), "en-US");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ("US", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity.
  address.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("canADA"), "en-US");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ("CA", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test country code detection.
  address.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("JP"), "en-US");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ("JP", address.country_code());
  EXPECT_EQ(ASCIIToUTF16("Japan"), country);

  // Test that we ignore unknown countries.
  address.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Unknown"), "en-US");
  country = address.GetInfo(ADDRESS_HOME_COUNTRY, "en-US");
  EXPECT_EQ(std::string(), address.country_code());
  EXPECT_EQ(string16(), country);
}

// Test that we properly match typed values to stored country data.
TEST_F(AddressTest, IsCountry) {
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
    FieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kValidMatches[i]), "US",
                             &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {
    "United",
    "Garbage"
  };
  for (size_t i = 0; i < arraysize(kInvalidMatches); ++i) {
    FieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kInvalidMatches[i]), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.set_country_code("");
  EXPECT_EQ(std::string(), address.country_code());
  FieldTypeSet matching_types;
  address.GetMatchingTypes(ASCIIToUTF16("Garbage"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}
