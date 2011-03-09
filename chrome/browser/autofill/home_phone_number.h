// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
#pragma once

#include "chrome/browser/autofill/phone_number.h"

class FormGroup;

class HomePhoneNumber : public PhoneNumber {
 public:
  HomePhoneNumber();
  explicit HomePhoneNumber(const HomePhoneNumber& phone);
  virtual ~HomePhoneNumber();

  HomePhoneNumber& operator=(const HomePhoneNumber& phone);

 protected:
  virtual AutofillFieldType GetNumberType() const;
  virtual AutofillFieldType GetCityCodeType() const;
  virtual AutofillFieldType GetCountryCodeType() const;
  virtual AutofillFieldType GetCityAndNumberType() const;
  virtual AutofillFieldType GetWholeNumberType() const;
};

#endif  // CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
