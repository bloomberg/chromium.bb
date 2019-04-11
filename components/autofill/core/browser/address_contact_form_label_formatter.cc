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
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, groups, field_types),
      form_has_street_address_(HasStreetAddress(field_types_for_labels())) {}

AddressContactFormLabelFormatter::~AddressContactFormLabelFormatter() {}

base::string16 AddressContactFormLabelFormatter::GetLabelForProfile(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  return focused_group == NAME || focused_group == ADDRESS_HOME
             ? GetLabelForProfileOnFocusedNameOrAddress(profile, focused_group)
             : GetLabelForProfileOnFocusedPhoneOrEmail(profile, focused_group);
}

// Note that the order in which parts of the label are added--national address
// or name and partial address in the top line followed by phone and email in
// the bottom line--ensures that the label is formatted correctly for
// |focused_group|, |focused_field_type_|, and for this kind of formatter.
//
// To format the address part of the label correctly, either the national
// address or a short form of the address is used.
//
// When a user is focused on a name field, the label has the national address:
// Name: |_________________________________________|
//       | Sarah Revere                            |
//       | 19 North Sq, Boston, MA 02113           |
//       | (617) 722-2000 • sarah1775@gmail.com    |
//       +-----------------------------------------+
//
// When a user is focused on an address field, the label has a short form of the
// address (A) to avoid showing data that also appears in the top line and (B)
// to reduce the width of the dropdown.
//
// The part that is displayed depends on whether |focused_field_type_| is
// related to street addresses.
//
// Address: |_________________________________________|
//          | 19 North Sq                             |
//          | Sarah Revere • Boston, MA 02113         |
//          | (617) 722-2000 • sarah1775@gmail.com    |
//          +-----------------------------------------+
//
// City: |_________________________________________|
//       | Boston                                  |
//       | Sarah Revere • 19 North Sq              |
//       | (617) 722-2000 • sarah1775@gmail.com    |
//       +-----------------------------------------+
base::string16
AddressContactFormLabelFormatter::GetLabelForProfileOnFocusedNameOrAddress(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::vector<base::string16> top_line_label_parts;

  if (focused_group != ADDRESS_HOME) {
    AddLabelPartIfNotEmpty(GetLabelNationalAddress(profile, app_locale(),
                                                   field_types_for_labels()),
                           &top_line_label_parts);
  }

  if (focused_group != NAME) {
    AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()),
                           &top_line_label_parts);
    AddLabelPartIfNotEmpty(GetLabelForFocusedAddress(
                               focused_field_type(), form_has_street_address_,
                               profile, app_locale(), field_types_for_labels()),
                           &top_line_label_parts);
  }

  std::vector<base::string16> bottom_line_label_parts;
  AddLabelPartIfNotEmpty(GetLabelPhone(profile, app_locale()),
                         &bottom_line_label_parts);
  AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()),
                         &bottom_line_label_parts);

  return ConstructLabelLines(ConstructLabelLine(top_line_label_parts),
                             ConstructLabelLine(bottom_line_label_parts));
}

// Note that the order in which parts of the label are added--name, phone and
// email in the top line followed by the national address in the bottom
// line--ensures that the label is formatted correctly for |focused_group| and
// for this kind of formatter.
base::string16
AddressContactFormLabelFormatter::GetLabelForProfileOnFocusedPhoneOrEmail(
    const AutofillProfile& profile,
    FieldTypeGroup focused_group) const {
  std::vector<base::string16> top_line_label_parts;

  AddLabelPartIfNotEmpty(GetLabelName(profile, app_locale()),
                         &top_line_label_parts);

  if (focused_group != PHONE_HOME) {
    AddLabelPartIfNotEmpty(GetLabelPhone(profile, app_locale()),
                           &top_line_label_parts);
  }

  if (focused_group != EMAIL) {
    AddLabelPartIfNotEmpty(GetLabelEmail(profile, app_locale()),
                           &top_line_label_parts);
  }

  // ExtractAddressFieldTypes ensures that name-related information is not
  // included in the result. National addresses have recipient names, e.g.
  // Sarah Revere, 19 North Sq, Boston, MA 02113. However, for this part of the
  // label, only 19 North Sq, Boston, MA 02113 is wanted.
  return ConstructLabelLines(
      ConstructLabelLine(top_line_label_parts),
      GetLabelNationalAddress(
          profile, app_locale(),
          ExtractAddressFieldTypes(field_types_for_labels())));
}

}  // namespace autofill
