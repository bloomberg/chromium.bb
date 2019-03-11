// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_phone_form_label_formatter.h"

namespace autofill {

AddressPhoneFormLabelFormatter::AddressPhoneFormLabelFormatter(
    const std::string& app_locale,
    FieldTypeGroup focused_group,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_group, field_types) {}

AddressPhoneFormLabelFormatter::~AddressPhoneFormLabelFormatter() {}

std::vector<base::string16> AddressPhoneFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  // TODO(crbug.com/936168): Implement GetLabels().
  std::vector<base::string16> labels;
  return labels;
}

}  // namespace autofill
