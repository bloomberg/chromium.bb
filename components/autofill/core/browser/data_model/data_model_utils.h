// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_

#include <string>

#include "base/strings/string16.h"

namespace autofill {

namespace data_util {

// Converts the integer |expiration_month| to base::string16. Returns a value
// between ["01"-"12"].
base::string16 Expiration2DigitMonthAsString(int expiration_month);

// Converts the integer |expiration_year| to base::string16. Returns a value
// between ["00"-"99"].
base::string16 Expiration2DigitYearAsString(int expiration_year);

// Converts the integer |expiration_year| to base::string16.
base::string16 Expiration4DigitYearAsString(int expiration_year);

// Converts a string representation of a month (such as "February" or "feb."
// or "2") into a numeric value in [1, 12]. Returns true on successful
// conversion or false if a month was not recognized. When conversion fails,
// |expiration_month| is not modified.
bool ParseExpirationMonth(const base::string16& text,
                          const std::string& app_locale,
                          int* expiration_month);

// Parses the |text| and stores the corresponding int value result in
// |*expiration_year|. This function accepts two digit years as well as
// four digit years between 2000 and 2999. Returns true on success.
// On failure, no change is made to |*expiration_year|.
bool ParseExpirationYear(const base::string16& text, int* expiration_year);

// Sets |*expiration_month| to |value| if |value| is a valid month (1-12).
// Returns if any change is made to |*expiration_month|.
bool SetExpirationMonth(int value, int* expiration_month);

// Sets |*expiration_year| to |value| if |value| is a valid year. See comments
// in the function body for the definition of "valid". Returns if any change is
// made to |*expiration_year|.
bool SetExpirationYear(int value, int* expiration_year);

}  // namespace data_util

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_DATA_MODEL_UTILS_H_
