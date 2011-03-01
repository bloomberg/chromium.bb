// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_
#define CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_
#pragma once

#include "chrome/browser/autofill/phone_number.h"

class FormGroup;

class FaxNumber : public PhoneNumber {
 public:
  FaxNumber();
  virtual ~FaxNumber();

  virtual FormGroup* Clone() const;

 protected:
  virtual AutofillFieldType GetNumberType() const;
  virtual AutofillFieldType GetCityCodeType() const;
  virtual AutofillFieldType GetCountryCodeType() const;
  virtual AutofillFieldType GetCityAndNumberType() const;
  virtual AutofillFieldType GetWholeNumberType() const;

 private:
  explicit FaxNumber(const FaxNumber& phone) : PhoneNumber(phone) {}
  void operator=(const FaxNumber& phone);
};

#endif  // CHROME_BROWSER_AUTOFILL_FAX_NUMBER_H_
