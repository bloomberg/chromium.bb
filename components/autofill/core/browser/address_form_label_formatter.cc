// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_form_label_formatter.h"

#include "components/autofill/core/browser/label_formatter_utils.h"

namespace autofill {

AddressFormLabelFormatter::AddressFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types)
    : LabelFormatter(app_locale, focused_field_type, field_types) {}

AddressFormLabelFormatter::~AddressFormLabelFormatter() {}

std::vector<base::string16> AddressFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  std::vector<base::string16> labels;
  for (const AutofillProfile* profile : profiles) {
    if (GetFocusedGroup() == ADDRESS_HOME) {
      labels.push_back(GetLabelName(*profile, app_locale()));
    } else {
      labels.push_back(GetLabelNationalAddress(*profile, app_locale(),
                                               field_types_for_labels()));
    }
  }
  return labels;
}

}  // namespace autofill
