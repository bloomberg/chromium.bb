// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_utils.h"

#include <memory>
#include <set>

#include "components/autofill/core/browser/address_form_label_formatter.h"
#include "components/autofill/core/browser/contact_form_label_formatter.h"

namespace autofill {
namespace {

// Returns true if |type| belongs to the NAME FieldTypeGroup. Note that billing
// ServerFieldTypes are converted to their non-billing types.
bool FieldTypeIsName(const ServerFieldType& type) {
  FieldTypeGroup focused_field_group =
      AutofillType(AutofillType(type).GetStorableType()).group();
  return focused_field_group == NAME;
}

}  // namespace

uint32_t DetermineGroups(const std::vector<ServerFieldType>& field_types) {
  uint32_t group_bitmask = 0;
  for (const ServerFieldType& type : field_types) {
    const FieldTypeGroup group =
        AutofillType(AutofillType(type).GetStorableType()).group();
    switch (group) {
      case autofill::NAME:
        group_bitmask |= label_formatter_groups::kName;
        break;
      case autofill::ADDRESS_HOME:
        group_bitmask |= label_formatter_groups::kAddress;
        break;
      case autofill::EMAIL:
        group_bitmask |= label_formatter_groups::kEmail;
        break;
      case autofill::PHONE_HOME:
        group_bitmask |= label_formatter_groups::kPhone;
        break;
      default:
        group_bitmask |= label_formatter_groups::kUnsupported;
    }
  }
  return group_bitmask;
}

std::unique_ptr<LabelFormatter> Create(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types) {
  if (!FieldTypeIsName(focused_field_type)) {
    return nullptr;
  }

  std::vector<ServerFieldType> filtered_field_types;
  for (const ServerFieldType& field_type : field_types) {
    // NO_SERVER_DATA fields are frequently found in the collection of field
    // types sent from the frontend. UKNOWN_TYPE fields represent various form
    // elements, e.g. checkboxes. Neither field type is useful to the formatter,
    // so they are excluded from its collection of field types.
    if (field_type != NO_SERVER_DATA && field_type != UNKNOWN_TYPE) {
      filtered_field_types.push_back(field_type);
    }
  }

  const uint32_t groups = DetermineGroups(filtered_field_types);
  if (groups ==
      (label_formatter_groups::kName | label_formatter_groups::kAddress)) {
    return std::make_unique<AddressFormLabelFormatter>(
        app_locale, focused_field_type, filtered_field_types);
  }

  if (groups ==
      (label_formatter_groups::kName | label_formatter_groups::kPhone |
       label_formatter_groups::kEmail)) {
    return std::make_unique<ContactFormLabelFormatter>(
        app_locale, focused_field_type, filtered_field_types);
  }
  return nullptr;
}
}  // namespace autofill
