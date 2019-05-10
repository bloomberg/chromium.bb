// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_form_label_formatter.h"

#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

AddressFormLabelFormatter::AddressFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, groups, field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())) {}

AddressFormLabelFormatter::~AddressFormLabelFormatter() {}

base::string16 AddressFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  if (focused_group != ADDRESS_HOME) {
    return GetLabelNationalAddress(profile, app_locale(),
                                   field_types_for_labels());
  } else {
    std::vector<base::string16> label_parts;
    AddLabelPartIfNotEmpty(GetLabelFullName(profile, app_locale()),
                           &label_parts);
    AddLabelPartIfNotEmpty(GetLabelForFocusedAddress(
                               focused_field_type(), form_has_street_address_,
                               profile, app_locale(), field_types_for_labels()),
                           &label_parts);
    return ConstructLabelLine(label_parts);
  }
}

}  // namespace autofill
