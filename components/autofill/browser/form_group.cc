// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/form_group.h"

namespace autofill {

void FormGroup::GetMatchingTypes(const base::string16& text,
                                 const std::string& app_locale,
                                 FieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  FieldTypeSet types;
  GetSupportedTypes(&types);
  for (FieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    // TODO(isherman): Matches are case-sensitive for now.  Let's keep an eye on
    // this and decide whether there are compelling reasons to add case-
    // insensitivity.
    if (GetInfo(*type, app_locale) == text)
      matching_types->insert(*type);
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
                                 FieldTypeSet* non_empty_types) const {
  FieldTypeSet types;
  GetSupportedTypes(&types);
  for (FieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    if (!GetInfo(*type, app_locale).empty())
      non_empty_types->insert(*type);
  }
}

base::string16 FormGroup::GetInfo(AutofillFieldType type,
                            const std::string& app_locale) const {
  return GetRawInfo(type);
}

bool FormGroup::SetInfo(AutofillFieldType type,
                        const base::string16& value,
                        const std::string& app_locale) {
  SetRawInfo(type, value);
  return true;
}

}  // namespace autofill
