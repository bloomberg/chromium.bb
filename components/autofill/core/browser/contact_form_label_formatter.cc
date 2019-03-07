// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_form_label_formatter.h"

namespace autofill {

ContactFormLabelFormatter::ContactFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types,
    const std::set<FieldTypeGroup>& field_type_groups)
    : LabelFormatter(app_locale, focused_field_type, field_types),
      field_type_groups_(field_type_groups),
      filtered_field_type_groups_(field_type_groups) {
  for (const ServerFieldType& type : field_types) {
    if (type != focused_field_type) {
      field_types_for_labels_.push_back(type);
    }
  }
  const FieldTypeGroup group =
      AutofillType(AutofillType(focused_field_type).GetStorableType()).group();
  filtered_field_type_groups_.erase(group);
}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

std::vector<base::string16> ContactFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  // TODO(crbug.com/936168): Implement GetLabels().
  std::vector<base::string16> labels;
  return labels;
}

}  // namespace autofill
