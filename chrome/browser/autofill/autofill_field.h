// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_

#include "chrome/browser/autofill/field_types.h"
#include "webkit/glue/form_field.h"

class AutoFillField : public webkit_glue::FormField {
 public:
  AutoFillField(const webkit_glue::FormField& field);

  const FieldTypeSet& possible_types() const { return possible_types_; }

  // The unique signature of this field, composed of the field name and the html
  // input type in a 32-bit hash.
  std::string FieldSignature() const;

 private:
   FieldTypeSet possible_types_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_
