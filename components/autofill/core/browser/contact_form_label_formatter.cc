// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_form_label_formatter.h"

namespace autofill {

ContactFormLabelFormatter::ContactFormLabelFormatter(
    const std::string& app_locale,
    ServerFieldType focused_field_type,
    const std::vector<ServerFieldType>& field_types)
    : app_locale_(app_locale),
      focused_field_type_(focused_field_type),
      field_types_(field_types) {
  for (const ServerFieldType& type : field_types) {
    if (type != focused_field_type_) {
      filtered_field_types_.push_back(type);
    }
  }
}

ContactFormLabelFormatter::~ContactFormLabelFormatter() {}

std::vector<base::string16> ContactFormLabelFormatter::GetLabels(
    const std::vector<AutofillProfile*>& profiles) const {
  // TODO(crbug.com/936168): Implement GetLabels().
  std::vector<base::string16> labels;
  return labels;
}

}  // namespace autofill
