// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_change.h"

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"

AutofillChange::AutofillChange(Type type, const AutofillKey& key)
    : GenericAutofillChange<AutofillKey>(type, key) {
}

AutofillChange::~AutofillChange() {
}

AutofillProfileChange::AutofillProfileChange(
  Type type, std::string key, const AutofillProfile* profile)
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

AutofillCreditCardChange::AutofillCreditCardChange(
  Type type, std::string key, const CreditCard* credit_card)
    : GenericAutofillChange<std::string>(type, key), credit_card_(credit_card) {
  DCHECK(type == ADD ? (credit_card && credit_card->guid() == key) : true);
  DCHECK(type == UPDATE ? (credit_card && credit_card->guid() == key) : true);
  DCHECK(type == REMOVE ? !credit_card : true);
}

AutofillCreditCardChange::~AutofillCreditCardChange() {
}

bool AutofillCreditCardChange::operator==(
    const AutofillCreditCardChange& change) const {
  return type() == change.type() &&
         key() == change.key() &&
         (type() != REMOVE) ? *credit_card() == *change.credit_card() : true;
}
