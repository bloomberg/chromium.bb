// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/form_field.h"

class AutofillScanner;

// A form field that can parse either a FullNameField or a FirstLastNameField.
class NameField : public FormField {
 public:
  static NameField* Parse(AutofillScanner* scanner, bool is_ecml);

 protected:
  NameField() {}

  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLast2);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLast);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLast2);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLastMiddleWithSpaces);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLastEmpty);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLastEmpty);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, MiddleInitial);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, MiddleInitialAtEnd);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, ECMLFirstMiddleLast);

  DISALLOW_COPY_AND_ASSIGN(NameField);
};

#endif  // CHROME_BROWSER_AUTOFILL_NAME_FIELD_H_
