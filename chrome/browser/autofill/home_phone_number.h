// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
#pragma once

#include "chrome/browser/autofill/phone_number.h"

class FormGroup;

class HomePhoneNumber : public PhoneNumber {
 public:
  HomePhoneNumber() {}
  virtual FormGroup* Clone() const;

 protected:
  virtual AutoFillFieldType GetNumberType() const;
  virtual AutoFillFieldType GetCityCodeType() const;
  virtual AutoFillFieldType GetCountryCodeType() const;
  virtual AutoFillFieldType GetCityAndNumberType() const;
  virtual AutoFillFieldType GetWholeNumberType() const;

 private:
  explicit HomePhoneNumber(const HomePhoneNumber& phone) : PhoneNumber(phone) {}
  void operator=(const HomePhoneNumber& phone);
};

#endif  // CHROME_BROWSER_AUTOFILL_HOME_PHONE_NUMBER_H_
