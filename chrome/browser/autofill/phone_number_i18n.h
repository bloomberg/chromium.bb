// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace i18n {
namespace phonenumbers {
class PhoneNumber;
}
}

// Utilities to process, normalize and compare international phone numbers.
namespace autofill_i18n {

// Most of the following functions require |locale| to operate. The |locale| is
// a ISO 3166 standard code ("US" for USA, "CZ" for Czech Republic, etc.).

// Normalizes phone number, by changing digits in the extended fonts
// (such as \xFF1x) into '0'-'9'. Also strips out non-digit characters.
string16 NormalizePhoneNumber(const string16& value,
                              const std::string& locale);

// Parses |value| to extract the components of a phone number.  |number|
// returns the local number, |city_code| returns the city code, and
// |country_code| returns country code. For some regions the |city_code| is
// empty.
// The parsing is based on current locale - |locale|.
// Separator characters are stripped before parsing the digits.
// Returns true if parsing was successful, false otherwise.
bool ParsePhoneNumber(const string16& value,
                      const std::string& locale,
                      string16* country_code,
                      string16* city_code,
                      string16* number) WARN_UNUSED_RESULT;

enum FullPhoneFormat {
  E164,  // Example: +16502345678
  INTERNATIONAL,  // Example: +1 650-234-5678
  NATIONAL,  // Example: (650) 234-5678
  RFC3966  // Example: +1-650-234-5678
};

// Constructs whole phone number from parts.
// |city_code| - area code, could be empty.
// |country_code| - country code, could be empty
// |number| - local number, should not be empty.
// |locale| - current locale, the parsing is based on.
// |phone_format| - whole number constructed in that format,
// |whole_number| - constructed whole number.
// Separator characters are stripped before parsing the digits.
// Returns true if parsing was successful, false otherwise.
bool ConstructPhoneNumber(const string16& country_code,
                          const string16& city_code,
                          const string16& number,
                          const std::string& locale,
                          FullPhoneFormat phone_format,
                          string16* whole_number) WARN_UNUSED_RESULT;

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& country_code);

// The cached phone number, does parsing only once, improves performance.
class PhoneObject {
 public:
  PhoneObject(const string16& number, const std::string& locale);
  PhoneObject(const PhoneObject&);
  PhoneObject();
  ~PhoneObject();

  string16 GetCountryCode() const { return country_code_; }
  string16 GetCityCode() const { return city_code_; }
  string16 GetNumber() const { return number_; }
  std::string GetLocale() const { return locale_; }

  string16 GetWholeNumber() const;

  PhoneObject& operator=(const PhoneObject& other);

  bool IsValidNumber() const { return i18n_number_ != NULL; }

 private:
  string16 city_code_;
  string16 country_code_;
  string16 number_;
  mutable string16 whole_number_;  // Set on first request.
  std::string locale_;
  scoped_ptr<i18n::phonenumbers::PhoneNumber> i18n_number_;
};

}  // namespace autofill_i18n

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
