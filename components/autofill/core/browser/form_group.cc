// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_group.h"

#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

void FormGroup::GetMatchingTypes(const base::string16& text,
                                 const std::string& app_locale,
                                 ServerFieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (ServerFieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    if (GetInfo(AutofillType(*type), app_locale) == text)
      matching_types->insert(*type);
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
                                 ServerFieldTypeSet* non_empty_types) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (ServerFieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    if (!GetInfo(AutofillType(*type), app_locale).empty())
      non_empty_types->insert(*type);
  }
}

base::string16 FormGroup::GetInfo(const AutofillType& type,
                                  const std::string& app_locale) const {
  return GetRawInfo(type.GetStorableType());
}

bool FormGroup::SetInfo(const AutofillType& type,
                        const base::string16& value,
                        const std::string& app_locale) {
  SetRawInfo(type.GetStorableType(), value);
  return true;
}

}  // namespace autofill
