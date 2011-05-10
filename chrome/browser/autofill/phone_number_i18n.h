// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"

// Utilities to process, normalize and compare international phone numbers.

namespace autofill_i18n {

// Most of the following functions require |locale| to operate. The |locale| is
// a ISO 3166 standard code ("US" for USA, "CZ" for Czech Republic, etc.).

// Normalizes phone number, by changing digits in the extended fonts
// (such as \xFF1x) into '0'-'9'. Also strips out non-digit characters.
string16 NormalizePhoneNumber(const string16& value);

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

// Parses and then formats phone number either nationally or internationally.
// |phone| - raw, unsanitized phone number, as entered by user.
// |locale| - country locale code, such as "US".
// |phone_format| - the phone type format.
// The same as calling ParsePhoneNumber(phone, locale, &country, &city,
// &number); ConstructPhoneNumber(country, city, number, locale, phone_format,
// &formatted_phone); but almost twice as fast, as parsing done only once.
// Returns formatted phone on success, empty string on error.
string16 FormatPhone(const string16& phone,
                     const std::string& locale,
                     FullPhoneFormat phone_format);

enum PhoneMatch {
  PHONES_NOT_EQUAL,
  PHONES_EQUAL,  // +1(650)2345678 is equal to 1-650-234-56-78.
  PHONES_SUBMATCH,  // 650-234-56-78 or 2345678 is a submatch for
                    // +1(650)2345678. The submatch is symmetric.
};

// Compares two phones, normalizing them before comparision.
PhoneMatch ComparePhones(const string16& phone1,
                         const string16& phone2,
                         const std::string& locale);

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& country_code);

}  // namespace autofill_i18n

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
