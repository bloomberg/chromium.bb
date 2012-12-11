// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
#define CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_

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

// Most of the following functions require |region| to operate. The |region| is
// a ISO 3166 standard code ("US" for USA, "CZ" for Czech Republic, etc.).

// Normalizes phone number, by changing digits in the extended fonts
// (such as \xFF1x) into '0'-'9'. Also strips out non-digit characters.
string16 NormalizePhoneNumber(const string16& value,
                              const std::string& region);

// Parses the number stored in |value| as a phone number interpreted in the
// given |region|, and stores the results into the remaining arguments.  The
// |region| should be a 2-letter country code.  This is an internal function,
// exposed in the header file so that it can be tested.
bool ParsePhoneNumber(const string16& value,
                      const std::string& region,
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
// |country_code| - country code, could be empty.
// |number| - local number, should not be empty.
// |region| - current region, the parsing is based on.
// |phone_format| - whole number constructed in that format,
// |whole_number| - constructed whole number.
// Separator characters are stripped before parsing the digits.
// Returns true if parsing was successful, false otherwise.
bool ConstructPhoneNumber(const string16& country_code,
                          const string16& city_code,
                          const string16& number,
                          const std::string& region,
                          FullPhoneFormat phone_format,
                          string16* whole_number) WARN_UNUSED_RESULT;

// Returns true if |number_a| and |number_b| parse to the same phone number in
// the given |region|.
bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& region);

// The cached phone number, does parsing only once, improves performance.
class PhoneObject {
 public:
  PhoneObject(const string16& number, const std::string& region);
  PhoneObject(const PhoneObject&);
  PhoneObject();
  ~PhoneObject();

  string16 GetCountryCode() const { return country_code_; }
  string16 GetCityCode() const { return city_code_; }
  string16 GetNumber() const { return number_; }
  std::string GetRegion() const { return region_; }

  string16 GetWholeNumber() const;

  PhoneObject& operator=(const PhoneObject& other);

  bool IsValidNumber() const { return i18n_number_ != NULL; }

 private:
  string16 city_code_;
  string16 country_code_;
  string16 number_;
  mutable string16 whole_number_;  // Set on first request.
  std::string region_;
  scoped_ptr<i18n::phonenumbers::PhoneNumber> i18n_number_;
};

}  // namespace autofill_i18n

#endif  // CHROME_BROWSER_AUTOFILL_PHONE_NUMBER_I18N_H_
