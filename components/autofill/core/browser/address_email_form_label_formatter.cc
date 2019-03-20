// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_email_form_label_formatter.h"

#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

AddressEmailFormLabelFormatter::AddressEmailFormLabelFormatter(
    const std::string& app_locale,
    FieldTypeGroup focused_group,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_group, field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())) {}

AddressEmailFormLabelFormatter::~AddressEmailFormLabelFormatter() {}

std::vector<base::string16> AddressEmailFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  std::vector<base::string16> labels;

  for (const AutofillProfile* profile : profiles) {
    switch (focused_group()) {
      case ADDRESS_HOME:
        labels.push_back(GetLabelForFocusedAddress(*profile));
        break;

      case EMAIL:
        labels.push_back(GetLabelForFocusedEmail(*profile));
        break;

      default:
        labels.push_back(GetLabelDefault(*profile));
    }
  }
  return labels;
}

base::string16 AddressEmailFormLabelFormatter::GetLabelForFocusedAddress(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()), &label_parts);
  AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()), &label_parts);
  return ConstructLabelLine(label_parts);
}

base::string16 AddressEmailFormLabelFormatter::GetLabelForFocusedEmail(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()), &label_parts);
  AddLabelPartIfNotEmpty(
      GetLabelAddress(form_has_street_address_, profile, app_locale(),
                      field_types_for_labels()),
      &label_parts);
  return ConstructLabelLine(label_parts);
}

base::string16 AddressEmailFormLabelFormatter::GetLabelDefault(
    const AutofillProfile& profile) const {
  std::vector<base::string16> label_parts;
  AddLabelPartIfNotEmpty(
      GetLabelAddress(form_has_street_address_, profile, app_locale(),
                      field_types_for_labels()),
      &label_parts);
  AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()), &label_parts);
  return ConstructLabelLine(label_parts);
}

}  // namespace autofill
