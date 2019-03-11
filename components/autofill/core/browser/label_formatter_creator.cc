// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_creator.h"

#include <memory>

#include "components/autofill/core/browser/address_contact_form_label_formatter.h"
#include "components/autofill/core/browser/address_email_form_label_formatter.h"
#include "components/autofill/core/browser/address_form_label_formatter.h"
#include "components/autofill/core/browser/address_phone_form_label_formatter.h"
#include "components/autofill/core/browser/contact_form_label_formatter.h"

namespace autofill {
namespace {

bool ContainsName(uint32_t groups) {
  return groups & label_formatter_groups::kName;
}

bool ContainsAddress(uint32_t groups) {
  return groups & label_formatter_groups::kAddress;
}

bool ContainsEmail(uint32_t groups) {
  return groups & label_formatter_groups::kEmail;
}

bool ContainsPhone(uint32_t groups) {
  return groups & label_formatter_groups::kPhone;
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
        break;
    }
  }
  return group_bitmask;
}

std::unique_ptr<LabelFormatter> Create(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types) {
  const uint32_t groups = DetermineGroups(field_types);

  if (!ContainsName(groups)) {
    return nullptr;
  }
  const FieldTypeGroup focused_group =
      AutofillType(AutofillType(focused_field_type).GetStorableType()).group();

  if (ContainsAddress(groups) && ContainsEmail(groups) &&
      ContainsPhone(groups)) {
    return std::make_unique<AddressContactFormLabelFormatter>(
        app_locale, focused_group, field_types);
  } else if (ContainsAddress(groups) && ContainsPhone(groups)) {
    return std::make_unique<AddressPhoneFormLabelFormatter>(
        app_locale, focused_group, field_types);
  } else if (ContainsAddress(groups) && ContainsEmail(groups)) {
    return std::make_unique<AddressEmailFormLabelFormatter>(
        app_locale, focused_group, field_types);
  } else if (ContainsAddress(groups)) {
    return std::make_unique<AddressFormLabelFormatter>(
        app_locale, focused_group, field_types);
  } else if (ContainsEmail(groups) || ContainsPhone(groups)) {
    return std::make_unique<ContactFormLabelFormatter>(
        app_locale, focused_group, field_types);
  } else {
    return nullptr;
  }
}

}  // namespace autofill
