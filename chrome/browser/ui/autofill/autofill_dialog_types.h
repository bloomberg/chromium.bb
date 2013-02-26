// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "third_party/skia/include/core/SkColor.h"

class AutofillField;

namespace autofill {

// This struct describes a single input control for the imperative autocomplete
// dialog.
struct DetailInput {
  // Multiple DetailInput structs with the same row_id go on the same row. The
  // actual order of the rows is determined by their order of appearance in
  // kBillingInputs.
  int row_id;
  AutofillFieldType type;
  // Placeholder text resource ID.
  int placeholder_text_rid;
  // The section suffix that the field must have to match up to this input.
  const char* section_suffix;
  // A number between 0 and 1.0 that describes how much of the horizontal space
  // in the row should be allotted to this input. 0 is equivalent to 1.
  float expand_weight;
  // When non-empty, indicates the value that should be pre-filled into the
  // input.
  string16 autofilled_value;
};

// Determines whether |input| and |field| match.
typedef base::Callback<bool(const DetailInput& input,
                            const AutofillField& field)> InputFieldComparator;

// Sections of the dialog --- all fields that may be shown to the user fit under
// one of these sections.
enum DialogSection {
  SECTION_EMAIL,
  // The Autofill-backed dialog uses separate CC and billing sections.
  SECTION_CC,
  SECTION_BILLING,
  // The wallet-backed dialog uses a combined CC and billing section.
  SECTION_CC_BILLING,
  SECTION_SHIPPING,
};

// Termination actions for the dialog.
enum DialogAction {
  ACTION_CANCEL,
  ACTION_SUBMIT,
};

// A notification to show in the autofill dialog. Ranges from information to
// seriously scary security messages, and will give you the color it should be
// displayed (if you ask it).
class DialogNotification {
 public:
  enum Type {
    NONE,
    EXPLANATORY_MESSAGE,
    REQUIRED_ACTION,
    SECURITY_WARNING,
    VALIDATION_ERROR,
    WALLET_ERROR,
    WALLET_PROMO,
  };

  DialogNotification();
  DialogNotification(Type type, const string16& display_text);

  // Returns the appropriate background or text color for the view's
  // notification area based on |type_|.
  SkColor GetBackgroundColor() const;
  SkColor GetTextColor() const;

  // Whether this notification has an arrow pointing up at the account chooser.
  bool HasArrow() const;

  // Whether this notifications has the "Save details to wallet" checkbox.
  bool HasCheckbox() const;

  const string16& display_text() const { return display_text_; }

 private:
  Type type_;
  string16 display_text_;
};

enum DialogSignedInState {
  REQUIRES_RESPONSE,
  REQUIRES_SIGN_IN,
  REQUIRES_PASSIVE_SIGN_IN,
  SIGNED_IN,
};

typedef std::vector<DetailInput> DetailInputs;
typedef std::map<const DetailInput*, string16> DetailOutputMap;

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_TYPES_H_
