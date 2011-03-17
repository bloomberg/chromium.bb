// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_SELECT_CONTROL_HANDLER_H_
#define CHROME_BROWSER_AUTOFILL_SELECT_CONTROL_HANDLER_H_
#pragma once

#include "chrome/browser/autofill/field_types.h"
#include "base/string16.h"

class FormGroup;

namespace webkit_glue {
struct FormField;
}  // namespace webkit_glue

namespace autofill {

// Fills a select-one control with the appropriate value from |form_group|.
// Finds the matching value for field types that we know contain different
// variations of a value, e.g., (tx, TX, Texas) or credit card expiration
// months, e.g., (04, April).
void FillSelectControl(const FormGroup& form_group,
                       AutofillFieldType type,
                       webkit_glue::FormField* field);

// Returns true if |value| is a valid US state name or abbreviation.  It is case
// insensitive.  Valid for US states only.
bool IsValidState(const string16& value);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_SELECT_CONTROL_HANDLER_H_
