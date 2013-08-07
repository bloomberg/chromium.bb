// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_FIELD_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_FIELD_INFO_H_

#include <string>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {

struct AutofillServerFieldInfo {
  // The predicted type returned by the Autofill server for this field.
  ServerFieldType field_type;
  // Default value to be used for the field (only applies to
  // FIELD_WITH_DEFAULT_TYPE field type)
  std::string default_value;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SERVER_FIELD_INFO_H_
