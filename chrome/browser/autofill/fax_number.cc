// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/fax_number.h"

FaxNumber::FaxNumber() {}

FaxNumber::~FaxNumber() {}

FormGroup* FaxNumber::Clone() const {
  return new FaxNumber(*this);
}

AutoFillFieldType FaxNumber::GetNumberType() const {
  return PHONE_FAX_NUMBER;
}

AutoFillFieldType FaxNumber::GetCityCodeType() const {
  return PHONE_FAX_CITY_CODE;
}

AutoFillFieldType FaxNumber::GetCountryCodeType() const {
  return PHONE_FAX_COUNTRY_CODE;
}

AutoFillFieldType FaxNumber::GetCityAndNumberType() const {
  return PHONE_FAX_CITY_AND_NUMBER;
}

AutoFillFieldType FaxNumber::GetWholeNumberType() const {
  return PHONE_FAX_WHOLE_NUMBER;
}
