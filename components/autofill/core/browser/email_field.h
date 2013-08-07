// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_EMAIL_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_EMAIL_FIELD_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/autofill/core/browser/form_field.h"

namespace autofill {

class EmailField : public FormField {
 public:
  static FormField* Parse(AutofillScanner* scanner);

 protected:
  // FormField:
  virtual bool ClassifyField(ServerFieldTypeMap* map) const OVERRIDE;

 private:
  explicit EmailField(const AutofillField* field);

  const AutofillField* field_;

  DISALLOW_COPY_AND_ASSIGN(EmailField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_EMAIL_FIELD_H_
