// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/home_address.h"

HomeAddress::HomeAddress() {}

HomeAddress::~HomeAddress() {}

FormGroup* HomeAddress::Clone() const {
  return new HomeAddress(*this);
}

AutoFillFieldType HomeAddress::GetLine1Type() const {
  return ADDRESS_HOME_LINE1;
}

AutoFillFieldType HomeAddress::GetLine2Type() const {
  return ADDRESS_HOME_LINE2;
}

AutoFillFieldType HomeAddress::GetAptNumType() const {
  return ADDRESS_HOME_APT_NUM;
}

AutoFillFieldType HomeAddress::GetCityType() const {
  return ADDRESS_HOME_CITY;
}

AutoFillFieldType HomeAddress::GetStateType() const {
  return ADDRESS_HOME_STATE;
}

AutoFillFieldType HomeAddress::GetZipCodeType() const {
  return ADDRESS_HOME_ZIP;
}

AutoFillFieldType HomeAddress::GetCountryType() const {
  return ADDRESS_HOME_COUNTRY;
}
