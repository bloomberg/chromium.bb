// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_

#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/field_types.h"

class FormGroup;

// A specialization of Address that identifies itself as a home address.
class HomeAddress : public Address {
 public:
  HomeAddress() {}
  FormGroup* Clone() const { return new HomeAddress(*this); }

 protected:
  virtual AutoFillFieldType GetLine1Type() const {
    return ADDRESS_HOME_LINE1;
  }

  virtual AutoFillFieldType GetLine2Type() const {
    return ADDRESS_HOME_LINE2;
  }

  virtual AutoFillFieldType GetApptNumType() const {
    return ADDRESS_HOME_APPT_NUM;
  }

  virtual AutoFillFieldType GetCityType() const {
    return ADDRESS_HOME_CITY;
  }

  virtual AutoFillFieldType GetStateType() const {
    return ADDRESS_HOME_STATE;
  }

  virtual AutoFillFieldType GetZipCodeType() const {
    return ADDRESS_HOME_ZIP;
  }

  virtual AutoFillFieldType GetCountryType() const {
    return ADDRESS_HOME_COUNTRY;
  }

 private:
  explicit HomeAddress(const HomeAddress& address) : Address(address) {}
  void operator=(const HomeAddress& address);  // Not implemented.
};

#endif  // CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_
