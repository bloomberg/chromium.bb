// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_SERVER_FIELD_INFO_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_SERVER_FIELD_INFO_H_

#include <string>

#include "chrome/browser/autofill/field_types.h"

struct AutofillServerFieldInfo {
  // The predicted type returned by the Autofill server for this field.
  AutofillFieldType field_type;
  // Default value to be used for the field (only applies to
  // FIELD_WITH_DEFAULT_TYPE field type)
  std::string default_value;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_SERVER_FIELD_INFO_H_
