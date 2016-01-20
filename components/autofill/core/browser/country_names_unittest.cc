// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_names.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

// Test mapping of localized country names to country codes.
TEST(CountryNamesTest, GetCountryCode) {
  // Basic mapping
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("United States"), "en_US"));
  EXPECT_EQ("CA", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("Canada"), "en_US"));

  // Case-insensitive mapping
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("united states"), "en_US"));

  // Country codes should map to themselves, independent of locale.
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("US"), "en_US"));
  EXPECT_EQ("HU", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("hu"), "en_US"));
  EXPECT_EQ("CA", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("CA"), "fr_CA"));
  EXPECT_EQ("MX", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("mx"), "fr_CA"));

  // Basic synonyms
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("United States of America"), "en_US"));
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("USA"), "en_US"));

  // Other locales
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("Estados Unidos"), "es"));
  EXPECT_EQ("IT", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("Italia"), "it"));
  EXPECT_EQ("DE", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("duitsland"), "nl"));

  // Should fall back to "en_US" locale if all else fails.
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("United States"), "es"));
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("united states"), "es"));
  EXPECT_EQ("US", CountryNames::GetInstance()->GetCountryCode(
                      ASCIIToUTF16("USA"), "es"));
}

// Test mapping of empty country name to country code.
TEST(CountryNamesTest, EmptyCountryNameHasEmptyCountryCode) {
  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(base::string16(), "en");
  EXPECT_TRUE(country_code.empty()) << country_code;
}

}  // namespace autofill
