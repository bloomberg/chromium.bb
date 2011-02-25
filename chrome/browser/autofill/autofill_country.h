// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_COUNTRY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_COUNTRY_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"

// Stores data associated with a country. Strings are localized to the app
// locale.
class AutoFillCountry {
 public:
  // Returns country data corresponding to the two-letter ISO code
  // |country_code|.
  AutoFillCountry(const std::string& country_code, const std::string& locale);
  ~AutoFillCountry();

  // Fills |country_codes| with a list of the available countries' codes.
  static void GetAvailableCountries(
      std::vector<std::string>* country_codes);

  // Returns the likely country code for |locale|, or "US" as a fallback if no
  // mapping from the locale is available.
  static const std::string CountryCodeForLocale(const std::string& locale);

  // Returns the country code corresponding to |country|, which should be a
  // country code or country name localized to |locale|.
  static const std::string GetCountryCode(const string16& country,
                                          const std::string& locale);

  // Returns the application locale.
  static const std::string ApplicationLocale();

  const std::string country_code() const { return country_code_; }
  const string16 name() const { return name_; }
  const string16 postal_code_label() const { return postal_code_label_; }
  const string16 state_label() const { return state_label_; }

 private:
  AutoFillCountry(const std::string& country_code,
                  const string16& name_,
                  const string16& postal_code_label_,
                  const string16& state_label_);

  // The two-letter ISO-3166 country code.
  std::string country_code_;

  // The country's name, localized to the app locale.
  string16 name_;

  // The localized label for the postal code (or zip code) field.
  string16 postal_code_label_;

  // The localized label for the state (or province, district, etc.) field.
  string16 state_label_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_COUNTRY_H_
