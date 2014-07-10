// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_ADDRESS_VALIDATOR_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_ADDRESS_VALIDATOR_H_

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_validator.h"

namespace autofill {

MATCHER_P(CountryCodeMatcher, country_code, "Checks an AddressData's country") {
  // |arg| is an AddressData object.
  return arg.region_code == country_code;
}

class MockAddressValidator : public ::i18n::addressinput::AddressValidator {
 public:
  MockAddressValidator();
  virtual ~MockAddressValidator();

  MOCK_METHOD1(LoadRules, void(const std::string& country_code));

  MOCK_CONST_METHOD3(ValidateAddress,
      ::i18n::addressinput::AddressValidator::Status(
          const ::i18n::addressinput::AddressData& address,
          const ::i18n::addressinput::AddressProblemFilter& filter,
          ::i18n::addressinput::AddressProblems* problems));

  MOCK_CONST_METHOD4(GetSuggestions,
      ::i18n::addressinput::AddressValidator::Status(
          const ::i18n::addressinput::AddressData& user_input,
          ::i18n::addressinput::AddressField focused_field,
          size_t suggestions_limit,
          std::vector< ::i18n::addressinput::AddressData>* suggestions));

  MOCK_CONST_METHOD1(CanonicalizeAdministrativeArea,
                     bool(::i18n::addressinput::AddressData* address_data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAddressValidator);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_ADDRESS_VALIDATOR_H_
