// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_EMAIL_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_EMAIL_FIELD_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autofill/form_field.h"

class EmailField : public FormField {
 public:
  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(FieldTypeMap* map) const OVERRIDE;

 private:
  explicit EmailField(const AutofillField* field);

  const AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(EmailField);
};

#endif  // CHROME_BROWSER_AUTOFILL_EMAIL_FIELD_H_
