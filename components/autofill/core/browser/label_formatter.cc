// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "components/autofill/core/browser/address_contact_form_label_formatter.h"
#include "components/autofill/core/browser/address_email_form_label_formatter.h"
#include "components/autofill/core/browser/address_form_label_formatter.h"
#include "components/autofill/core/browser/address_phone_form_label_formatter.h"
#include "components/autofill/core/browser/contact_form_label_formatter.h"
#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

using label_formatter_groups::kAddress;
using label_formatter_groups::kEmail;
using label_formatter_groups::kName;
using label_formatter_groups::kPhone;

LabelFormatter::LabelFormatter(const std::string& app_locale,
                               ServerFieldType focused_field_type,
                               const std::vector<ServerFieldType>& field_types)
    : app_locale_(app_locale), focused_field_type_(focused_field_type) {
  std::set<FieldTypeGroup> groups{NAME, ADDRESS_HOME, EMAIL, PHONE_HOME};
  groups.erase(GetFocusedGroup());

  auto can_be_shown_in_label = [&groups](ServerFieldType type) -> bool {
    return groups.find(
               AutofillType(AutofillType(type).GetStorableType()).group()) !=
               groups.end() &&
           type != ADDRESS_HOME_COUNTRY && type != ADDRESS_BILLING_COUNTRY;
  };
  std::copy_if(field_types.begin(), field_types.end(),
               std::back_inserter(field_types_for_labels_),
               can_be_shown_in_label);
}

LabelFormatter::~LabelFormatter() = default;

FieldTypeGroup LabelFormatter::GetFocusedGroup() const {
  return AutofillType(AutofillType(focused_field_type_).GetStorableType())
      .group();
}

// static
std::unique_ptr<LabelFormatter> LabelFormatter::Create(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types) {
  const uint32_t groups = DetermineGroups(field_types);

  switch (groups) {
    case kName | kAddress | kEmail | kPhone:
      return std::make_unique<AddressContactFormLabelFormatter>(
          app_locale, focused_field_type, field_types);
    case kName | kAddress | kPhone:
      return std::make_unique<AddressPhoneFormLabelFormatter>(
          app_locale, focused_field_type, field_types);
    case kName | kAddress | kEmail:
      return std::make_unique<AddressEmailFormLabelFormatter>(
          app_locale, focused_field_type, field_types);
    case kName | kAddress:
      return std::make_unique<AddressFormLabelFormatter>(
          app_locale, focused_field_type, field_types);
    case kName | kEmail | kPhone:
    case kName | kEmail:
    case kName | kPhone:
      return std::make_unique<ContactFormLabelFormatter>(
          app_locale, focused_field_type, groups, field_types);
    default:
      return nullptr;
  }
}

}  // namespace autofill
