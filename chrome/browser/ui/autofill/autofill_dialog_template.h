// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TEMPLATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TEMPLATE_H_

#include "chrome/browser/autofill/field_types.h"

#include <cstddef>

namespace autofill {

// This struct describes a single input control for the imperative autocomplete
// dialog.
struct DetailInput {
  // Multiple DetailInput structs with the same row_id go on the same row. The
  // actual order of the rows is determined by their order of appearance in
  // kBillingInputs.
  int row_id;
  AutofillFieldType type;
  // TODO(estade): remove this, do l10n.
  const char* placeholder_text;
  // The section suffix that the field must have to match up to this input.
  const char* section_suffix;
  // A number between 0 and 1.0 that describes how much of the horizontal space
  // in the row should be allotted to this input. 0 is equivalent to 1.
  float expand_weight;
};

extern const DetailInput kEmailInputs[];
extern const size_t kEmailInputsSize;

extern const DetailInput kCCInputs[];
extern const size_t kCCInputsSize;

extern const DetailInput kBillingInputs[];
extern const size_t kBillingInputsSize;

extern const DetailInput kShippingInputs[];
extern const size_t kShippingInputsSize;

}  // namespace

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TEMPLATE_H_
