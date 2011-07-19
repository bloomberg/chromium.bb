// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test the constructor and accessors
TEST(AutofillCountryTest, AutofillCountry) {
  AutofillCountry united_states_en("US", "en_US");
  EXPECT_EQ("US", united_states_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), united_states_en.name());
  EXPECT_EQ(ASCIIToUTF16("ZIP code"), united_states_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("State"), united_states_en.state_label());

  AutofillCountry united_states_es("US", "es");
  EXPECT_EQ("US", united_states_es.country_code());
  EXPECT_EQ(ASCIIToUTF16("Estados Unidos"), united_states_es.name());

  AutofillCountry canada_en("CA", "en_US");
  EXPECT_EQ("CA", canada_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("Canada"), canada_en.name());
  EXPECT_EQ(ASCIIToUTF16("Postal code"), canada_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("Province"), canada_en.state_label());

  AutofillCountry canada_hu("CA", "hu");
  EXPECT_EQ("CA", canada_hu.country_code());
  EXPECT_EQ(ASCIIToUTF16("Kanada"), canada_hu.name());
}

// Test locale to country code mapping.
TEST(AutofillCountryTest, CountryCodeForLocale) {
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("en_US"));
  EXPECT_EQ("CA", AutofillCountry::CountryCodeForLocale("fr_CA"));
  EXPECT_EQ("FR", AutofillCountry::CountryCodeForLocale("fr"));
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("Unknown"));
  // "es-419" isn't associated with a country. See base/l10n/l10n_util.cc
  // for details about this locale. Default to US.
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("es-419"));
}

// Test mapping of localized country names to country codes.
TEST(AutofillCountryTest, GetCountryCode) {
  // Basic mapping
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("United States"),
                                                  "en_US"));
  EXPECT_EQ("CA", AutofillCountry::GetCountryCode(ASCIIToUTF16("Canada"),
                                                  "en_US"));

  // Case-insensitive mapping
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("united states"),
                                                  "en_US"));

  // Country codes should map to themselves, independent of locale.
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("US"), "en_US"));
  EXPECT_EQ("HU", AutofillCountry::GetCountryCode(ASCIIToUTF16("hu"), "en_US"));
  EXPECT_EQ("CA", AutofillCountry::GetCountryCode(ASCIIToUTF16("CA"), "fr_CA"));
  EXPECT_EQ("MX", AutofillCountry::GetCountryCode(ASCIIToUTF16("mx"), "fr_CA"));

  // Basic synonyms
  EXPECT_EQ("US",
            AutofillCountry::GetCountryCode(
                ASCIIToUTF16("United States of America"), "en_US"));
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("USA"),
                                                  "en_US"));

  // Other locales
  EXPECT_EQ("US",
            AutofillCountry::GetCountryCode(ASCIIToUTF16("Estados Unidos"),
                                            "es"));
  EXPECT_EQ("IT", AutofillCountry::GetCountryCode(ASCIIToUTF16("Italia"),
                                                  "it"));
  EXPECT_EQ("DE", AutofillCountry::GetCountryCode(ASCIIToUTF16("duitsland"),
                                                  "nl"));

  // Should fall back to "en_US" locale if all else fails.
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("United States"),
                                                  "es"));
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("united states"),
                                                  "es"));
  EXPECT_EQ("US", AutofillCountry::GetCountryCode(ASCIIToUTF16("USA"), "es"));
}
