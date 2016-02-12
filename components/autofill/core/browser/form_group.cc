// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_group.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/autofill_l10n_util.h"

namespace autofill {

void FormGroup::GetMatchingTypes(const base::string16& text,
                                 const std::string& app_locale,
                                 ServerFieldTypeSet* matching_types) const {
  if (text.empty()) {
    matching_types->insert(EMPTY_TYPE);
    return;
  }

  base::string16 canonicalized_text =
      AutofillProfile::CanonicalizeProfileString(text);
  if (canonicalized_text.empty())
    return;

  // TODO(crbug.com/574086): Investigate whether to use |app_locale| in case
  // insensitive comparisons.
  l10n::CaseInsensitiveCompare compare;
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (const auto& type : types) {
    if (compare.StringsEqual(canonicalized_text,
                             AutofillProfile::CanonicalizeProfileString(
                                 GetInfo(AutofillType(type), app_locale))))
      matching_types->insert(type);
  }
}

void FormGroup::GetNonEmptyTypes(const std::string& app_locale,
                                 ServerFieldTypeSet* non_empty_types) const {
  ServerFieldTypeSet types;
  GetSupportedTypes(&types);
  for (const auto& type : types) {
    if (!GetInfo(AutofillType(type), app_locale).empty())
      non_empty_types->insert(type);
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
