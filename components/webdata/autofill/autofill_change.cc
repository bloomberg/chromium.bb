// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/autofill/autofill_change.h"

#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"

AutofillChange::AutofillChange(Type type, const AutofillKey& key)
    : GenericAutofillChange<AutofillKey>(type, key) {
}

AutofillChange::~AutofillChange() {
}

AutofillProfileChange::AutofillProfileChange(
  Type type, const std::string& key, const AutofillProfile* profile)
    : GenericAutofillChange<std::string>(type, key),
      profile_(profile) {
  DCHECK(type == ADD ? (profile && profile->guid() == key) : true);
  DCHECK(type == UPDATE ? (profile && profile->guid() == key) : true);
  DCHECK(type == REMOVE ? !profile : true);
}

AutofillProfileChange::~AutofillProfileChange() {
}

bool AutofillProfileChange::operator==(
    const AutofillProfileChange& change) const {
  return type() == change.type() &&
         key() == change.key() &&
         (type() != REMOVE) ? *profile() == *change.profile() : true;
}
