// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_form_label_formatter.h"

#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

ContactFormLabelFormatter::ContactFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, field_types),
      groups_(groups) {}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

std::vector<base::string16> ContactFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  std::vector<base::string16> labels;

  for (const AutofillProfile* profile : profiles) {
    switch (GetFocusedGroup()) {
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

base::string16 ContactFormLabelFormatter::MaybeGetEmail(
    const AutofillProfile& profile) const {
  return ContainsEmail(groups_) ? GetLabelEmail(profile, app_locale())
                                : base::string16();
}

base::string16 ContactFormLabelFormatter::MaybeGetPhone(
    const AutofillProfile& profile) const {
  return ContainsPhone(groups_) ? GetLabelPhone(profile, app_locale())
                                : base::string16();
}

base::string16 ContactFormLabelFormatter::GetLabelForFocusedEmail(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(MaybeGetPhone(profile), &label_parts);
  AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()), &label_parts);
  return ConstructLabelLine(label_parts);
}

base::string16 ContactFormLabelFormatter::GetLabelForFocusedPhone(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()), &label_parts);
  AddLabelPartIfNotEmpty(MaybeGetEmail(profile), &label_parts);
  return ConstructLabelLine(label_parts);
}

base::string16 ContactFormLabelFormatter::GetLabelDefault(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(MaybeGetPhone(profile), &label_parts);
  AddLabelPartIfNotEmpty(MaybeGetEmail(profile), &label_parts);
  return ConstructLabelLine(label_parts);
}

}  // namespace autofill
