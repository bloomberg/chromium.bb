// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_contact_form_label_formatter.h"

#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

AddressContactFormLabelFormatter::AddressContactFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    uint32_t groups,
    const std::vector<ServerFieldType>& field_types,
    bool show_phone,
    bool show_email)
    : LabelFormatter(app_locale, focused_field_type, groups, field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())),
      show_phone_(show_phone),
      show_email_(show_email) {}

AddressContactFormLabelFormatter::~AddressContactFormLabelFormatter() {}

// Note that the order in which parts of the label are added--name, street
// address, phone, and email--ensures that the label is formatted correctly for
// |focused_group|, |focused_field_type_|, and this kind of formatter.
base::string16 AddressContactFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::vector<base::string16> label_parts;

  bool street_address_is_focused = focused_group == ADDRESS_HOME &&
                                   IsStreetAddressPart(focused_field_type());
  bool non_street_address_is_focused =
      focused_group == ADDRESS_HOME &&
      !IsStreetAddressPart(focused_field_type());

  if (focused_group != NAME && !non_street_address_is_focused) {
    AddLabelPartIfNotEmpty(GetLabelFullName(profile, app_locale()),
                           &label_parts);
  }

  if (!street_address_is_focused) {
    AddLabelPartIfNotEmpty(
        GetLabelAddress(form_has_street_address_, profile, app_locale(),
                        field_types_for_labels()),
        &label_parts);
  }

  if (focused_group != PHONE_HOME && show_phone_) {
    AddLabelPartIfNotEmpty(GetLabelPhone(profile, app_locale()), &label_parts);
  }

  if (focused_group != EMAIL && show_email_) {
    AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()), &label_parts);
  }

  return ConstructLabelLine(label_parts);
}

}  // namespace autofill
