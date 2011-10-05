// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/autofill/form_group.h"

#include <iterator>

void FormGroup::GetMatchingTypes(const string16& text,
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
    if (GetInfo(*type) == text)
      matching_types->insert(*type);
  }
}

void FormGroup::GetNonEmptyTypes(FieldTypeSet* non_empty_types) const {
  FieldTypeSet types;
  GetSupportedTypes(&types);
  for (FieldTypeSet::const_iterator type = types.begin();
       type != types.end(); ++type) {
    if (!GetInfo(*type).empty())
      non_empty_types->insert(*type);
  }
}

string16 FormGroup::GetCanonicalizedInfo(AutofillFieldType type) const {
  return GetInfo(type);
}

bool FormGroup::SetCanonicalizedInfo(AutofillFieldType type,
                                     const string16& value) {
  SetInfo(type, value);
  return true;
}
