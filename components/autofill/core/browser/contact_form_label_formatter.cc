// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_form_label_formatter.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/label_formatter_utils.h"
#include "components/autofill/core/browser/validation.h"

namespace autofill {

ContactFormLabelFormatter::ContactFormLabelFormatter(
    const std::string& app_locale,
    FieldTypeGroup focused_group,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_group, field_types), groups_(groups) {}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

std::vector<base::string16> ContactFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  std::vector<base::string16> labels;

  for (const AutofillProfile* profile : profiles) {
    switch (focused_group()) {
      case EMAIL:
        labels.push_back(GetLabelForFocusedEmail(*profile));
        break;

      case PHONE_HOME:
        labels.push_back(GetLabelForFocusedPhone(*profile));
        break;

      default:
        labels.push_back(GetLabelDefault(*profile));
    }
  }
  return labels;
}

void ContactFormLabelFormatter::MaybeAddEmail(
    const AutofillProfile& profile,
    std::vector<base::string16>* label_parts) const {
  const base::string16 email = ContainsEmail(groups_)
                                   ? GetLabelEmail(profile, app_locale())
                                   : base::string16();
  if (!email.empty() && IsValidEmailAddress(email)) {
    label_parts->push_back(email);
  }
}

void ContactFormLabelFormatter::MaybeAddPhone(
    const AutofillProfile& profile,
    std::vector<base::string16>* label_parts) const {
  const base::string16 phone = ContainsPhone(groups_)
                                   ? GetLabelPhone(profile, app_locale())
                                   : base::string16();
  if (!phone.empty()) {
    label_parts->push_back(phone);
  }
}

base::string16 ContactFormLabelFormatter::GetLabelForFocusedEmail(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  MaybeAddPhone(profile, &label_parts);

  const base::string16 name = GetLabelName(profile, app_locale());
  if (!name.empty()) {
    label_parts.push_back(name);
  }
  return base::JoinString(label_parts, base::WideToUTF16(kLabelDelimiter));
}

base::string16 ContactFormLabelFormatter::GetLabelForFocusedPhone(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  const base::string16 name = GetLabelName(profile, app_locale());
  if (!name.empty()) {
    label_parts.push_back(name);
  }
  MaybeAddEmail(profile, &label_parts);
  return base::JoinString(label_parts, base::WideToUTF16(kLabelDelimiter));
}

base::string16 ContactFormLabelFormatter::GetLabelDefault(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  MaybeAddPhone(profile, &label_parts);
  MaybeAddEmail(profile, &label_parts);
  return base::JoinString(label_parts, base::WideToUTF16(kLabelDelimiter));
}

}  // namespace autofill
