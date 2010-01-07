// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_

#include "chrome/browser/autofill/phone_number.h"

class FormGroup;

class HomePhoneNumber : public PhoneNumber {
 public:
  HomePhoneNumber() {}
  virtual FormGroup* Clone() const { return new HomePhoneNumber(*this); }

 protected:
  virtual AutoFillFieldType GetNumberType() const {
    return PHONE_HOME_NUMBER;
  }

  virtual AutoFillFieldType GetCityCodeType() const {
    return PHONE_HOME_CITY_CODE;
  }

  virtual AutoFillFieldType GetCountryCodeType() const {
    return PHONE_HOME_COUNTRY_CODE;
  }

  virtual AutoFillFieldType GetCityAndNumberType() const {
    return PHONE_HOME_CITY_AND_NUMBER;
  }

  virtual AutoFillFieldType GetWholeNumberType() const {
    return PHONE_HOME_WHOLE_NUMBER;
  }

 private:
  explicit HomePhoneNumber(const HomePhoneNumber& phone) : PhoneNumber(phone) {}
  void operator=(const HomePhoneNumber& phone);
};

#endif  // CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
