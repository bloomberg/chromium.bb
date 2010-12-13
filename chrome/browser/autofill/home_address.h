// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_
#pragma once

#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/field_types.h"

class FormGroup;

// A specialization of Address that identifies itself as a home address.
class HomeAddress : public Address {
 public:
  HomeAddress();
  virtual ~HomeAddress();
  virtual FormGroup* Clone() const;

 protected:
  virtual AutoFillFieldType GetLine1Type() const;
  virtual AutoFillFieldType GetLine2Type() const;
  virtual AutoFillFieldType GetAptNumType() const;
  virtual AutoFillFieldType GetCityType() const;
  virtual AutoFillFieldType GetStateType() const;
  virtual AutoFillFieldType GetZipCodeType() const;
  virtual AutoFillFieldType GetCountryType() const;

 private:
  explicit HomeAddress(const HomeAddress& address) : Address(address) {}
  void operator=(const HomeAddress& address);  // Not implemented.
};

#endif  // CHROME_BROWSER_AUTOFILL_HOME_ADDRESS_H_
