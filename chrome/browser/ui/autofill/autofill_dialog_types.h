// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_

#include <map>
#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "third_party/skia/include/core/SkColor.h"

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
  // When non-empty, indicates the value that should be pre-filled into the
  // input.
  string16 autofilled_value;
};

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections.
enum DialogSection {
  SECTION_EMAIL,
  SECTION_CC,
  SECTION_BILLING,
  SECTION_SHIPPING,
};

// Termination actions for the dialog.
enum DialogAction {
  ACTION_ABORT,
  ACTION_SUBMIT,
};

// A notification to show in the autofill dialog. Ranges from information to
// seriously scary security messages, and will give you the color it should be
// displayed (if you ask it).
class DialogNotification {
 public:
  enum Type {
    NONE,
    REQUIRED_ACTION,
    SECURITY_WARNING,
    SUBMISSION_OPTION,
    VALIDATION_ERROR,
    WALLET_ERROR,
  };

  DialogNotification();
  DialogNotification(Type type, const string16& display_text);

  // Returns the appropriate background color for the view's notification area
  // based on |type_|.
  SkColor GetBackgroundColor() const;

  const string16& display_text() const { return display_text_; }

 private:
  Type type_;
  string16 display_text_;
};

typedef std::vector<DetailInput> DetailInputs;
typedef std::map<const DetailInput*, string16> DetailOutputMap;

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
