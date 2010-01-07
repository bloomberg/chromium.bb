// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_

#include "chrome/browser/autofill/phone_number.h"

class FormGroup;

class FaxNumber : public PhoneNumber {
 public:
  FaxNumber() {}
  virtual FormGroup* Clone() const { return new FaxNumber(*this); }

 protected:
  virtual AutoFillFieldType GetNumberType() const {
    return PHONE_FAX_NUMBER;
  }

  virtual AutoFillFieldType GetCityCodeType() const {
    return PHONE_FAX_CITY_CODE;
  }

  virtual AutoFillFieldType GetCountryCodeType() const {
    return PHONE_FAX_COUNTRY_CODE;
  }

  virtual AutoFillFieldType GetCityAndNumberType() const {
    return PHONE_FAX_CITY_AND_NUMBER;
  }

  virtual AutoFillFieldType GetWholeNumberType() const {
    return PHONE_FAX_WHOLE_NUMBER;
  }

 private:
  explicit FaxNumber(const FaxNumber& phone) : PhoneNumber(phone) {}
  void operator=(const FaxNumber& phone);
};

#endif  // CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_
