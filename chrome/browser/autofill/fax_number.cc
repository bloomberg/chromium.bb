// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/fax_number.h"

FaxNumber::FaxNumber() {}

FaxNumber::FaxNumber(const FaxNumber& fax) : PhoneNumber(fax) {}

FaxNumber::~FaxNumber() {}

FaxNumber& FaxNumber::operator=(const FaxNumber& fax) {
  PhoneNumber::operator=(fax);
  return *this;
}

AutofillFieldType FaxNumber::GetNumberType() const {
  return PHONE_FAX_NUMBER;
}

AutofillFieldType FaxNumber::GetCityCodeType() const {
  return PHONE_FAX_CITY_CODE;
}

AutofillFieldType FaxNumber::GetCountryCodeType() const {
  return PHONE_FAX_COUNTRY_CODE;
}

AutofillFieldType FaxNumber::GetCityAndNumberType() const {
  return PHONE_FAX_CITY_AND_NUMBER;
}

AutofillFieldType FaxNumber::GetWholeNumberType() const {
  return PHONE_FAX_WHOLE_NUMBER;
}
