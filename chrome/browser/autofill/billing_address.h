// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_BILLING_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_BILLING_ADDRESS_H_
#pragma once

#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/field_types.h"

class FormGroup;

// A specialization of Address that identifies itself as a billing address.
class BillingAddress : public Address {
 public:
  BillingAddress() {}
  FormGroup* Clone() const { return new BillingAddress(*this); }

 protected:
  virtual AutoFillFieldType GetLine1Type() const {
    return ADDRESS_BILLING_LINE1;
  }

  virtual AutoFillFieldType GetLine2Type() const {
    return ADDRESS_BILLING_LINE2;
  }

  virtual AutoFillFieldType GetAptNumType() const {
    return ADDRESS_BILLING_APT_NUM;
  }

  virtual AutoFillFieldType GetCityType() const {
    return ADDRESS_BILLING_CITY;
  }

  virtual AutoFillFieldType GetStateType() const {
    return ADDRESS_BILLING_STATE;
  }

  virtual AutoFillFieldType GetZipCodeType() const {
    return ADDRESS_BILLING_ZIP;
  }

  virtual AutoFillFieldType GetCountryType() const {
    return ADDRESS_BILLING_COUNTRY;
  }

 private:
  explicit BillingAddress(const BillingAddress& address) : Address(address) {}
  void operator=(const BillingAddress& address);  // Not implemented.
};

#endif  // CHROME_BROWSER_AUTOFILL_BILLING_ADDRESS_H_
