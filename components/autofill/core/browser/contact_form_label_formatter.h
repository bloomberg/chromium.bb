// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_

#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/label_formatter.h"

namespace autofill {

// A LabelFormatter that creates Suggestions' disambiguating labels for forms
// containing name and phone or email fields.
class ContactFormLabelFormatter : public LabelFormatter {
 public:
  ContactFormLabelFormatter(const std::string& app_locale,
                            FieldTypeGroup focused_group,
                            uint32_t groups,
                            const std::vector<ServerFieldType>& field_types);

  ~ContactFormLabelFormatter() override;

  std::vector<base::string16> GetLabels(
      const std::vector<AutofillProfile*>& profiles) const override;

 private:
  // Adds |profile|'s email address to |label_parts| if |profile| has a valid
  // email address and if this formatter's associated form has an email field.
  void MaybeAddEmail(const AutofillProfile& profile,
                     std::vector<base::string16>* label_parts) const;

  // Adds |profile|'s phone number to |label_parts| if |profile| has a phone
  // number and if this formatter's associated form has a phone field.
  void MaybeAddPhone(const AutofillProfile& profile,
                     std::vector<base::string16>* label_parts) const;

  // Returns a label to show the user when the focused field is related to
  // phone numbers.
  base::string16 GetLabelForFocusedPhone(const AutofillProfile& profile) const;

  // Returns a label to show the user when the focused field is related to
  // email addresses.
  base::string16 GetLabelForFocusedEmail(const AutofillProfile& profile) const;

  // Returns a label to show the user when the focused field is related to
  // neither phone numbers nor email addresses. This is used, for example, when
  // the user focuses on a name-related field.
  base::string16 GetLabelDefault(const AutofillProfile& profile) const;

  // A bitmask indicating which fields the form contains.
  uint32_t groups_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CONTACT_FORM_LABEL_FORMATTER_H_
