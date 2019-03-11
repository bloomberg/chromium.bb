// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter.h"

#include <algorithm>
#include <iterator>
#include <set>

namespace autofill {

LabelFormatter::LabelFormatter(const std::string& app_locale,
                               FieldTypeGroup focused_group,
                               const std::vector<ServerFieldType>& field_types)
    : app_locale_(app_locale), focused_group_(focused_group) {
  std::set<FieldTypeGroup> groups{NAME, ADDRESS_HOME, EMAIL, PHONE_HOME};
  groups.erase(focused_group_);

  auto can_be_shown_in_label = [&groups](ServerFieldType type) -> bool {
    return groups.find(
               AutofillType(AutofillType(type).GetStorableType()).group()) !=
               groups.end() &&
           type != ADDRESS_HOME_COUNTRY && type != ADDRESS_BILLING_COUNTRY;
  };

  std::copy_if(field_types.begin(), field_types.end(),
               back_inserter(field_types_for_labels_), can_be_shown_in_label);
}

LabelFormatter::~LabelFormatter() = default;

}  // namespace autofill
