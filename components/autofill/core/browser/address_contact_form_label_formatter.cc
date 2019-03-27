// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_contact_form_label_formatter.h"

namespace autofill {

AddressContactFormLabelFormatter::AddressContactFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, field_types) {}

AddressContactFormLabelFormatter::~AddressContactFormLabelFormatter() {}

base::string16 AddressContactFormLabelFormatter::GetLabelForFocusedGroup(
    const AutofillProfile& profile,
    FieldTypeGroup group) const {
  // TODO(crbug.com/936168): Implement GetLabelForFocusedGroup().
  return base::string16();
}

}  // namespace autofill
