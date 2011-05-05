// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number_i18n.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/libphonenumber/cpp/src/phonenumberutil.h"

using namespace i18n::phonenumbers;

namespace autofill_i18n {

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& country_code) {
  DCHECK(country_code.size() == 0 || country_code.size() == 2);
  std::string safe_country_code(country_code);
  if (safe_country_code.size() == 0)
    safe_country_code = "US";

  PhoneNumberUtil *phone_util = PhoneNumberUtil::GetInstance();
  PhoneNumber phone_number_a;
  phone_util->Parse(UTF16ToUTF8(number_a), safe_country_code, &phone_number_a);
  PhoneNumber phone_number_b;
  phone_util->Parse(UTF16ToUTF8(number_b), safe_country_code, &phone_number_b);

  PhoneNumberUtil::MatchType match = phone_util->IsNumberMatch(phone_number_a,
                                                               phone_number_b);
  // Allow |NSN_MATCH| for implied country code if one is not set.
  return match == PhoneNumberUtil::NSN_MATCH ||
         match == PhoneNumberUtil::EXACT_MATCH;
}

}  // namespace autofill_i18n
