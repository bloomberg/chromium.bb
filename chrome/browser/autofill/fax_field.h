// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FAX_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_FAX_FIELD_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_field.h"

class AutoFillField;

// A class that identifies itself as a fax field using heuristics.
class FaxField : public FormField {
 public:
  static FaxField* Parse(std::vector<AutoFillField*>::const_iterator* iter);

  virtual bool GetFieldInfo(FieldTypeMap* field_type_map) const;

 private:
  FaxField();

  // The fax number field.
  AutoFillField* number_;

  DISALLOW_COPY_AND_ASSIGN(FaxField);
};

#endif  // CHROME_BROWSER_AUTOFILL_FAX_FIELD_H_
