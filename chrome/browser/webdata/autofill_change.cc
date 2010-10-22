// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_change.h"

AutofillProfileChange::AutofillProfileChange(Type t,
                                             string16 k,
                                             const AutoFillProfile* p,
                                             const string16& pre_update_label)
    : GenericAutofillChange<string16>(t, k),
      profile_(p),
      pre_update_label_(pre_update_label) {
}

AutofillProfileChange::~AutofillProfileChange() {}

bool AutofillProfileChange::operator==(
    const AutofillProfileChange& change) const {
  if (type() != change.type() || key() != change.key())
    return false;
  if (type() == REMOVE)
    return true;
  // TODO(dhollowa): Replace with |AutoFillProfile::Compare|.
  // http://crbug.com/58813
  if (*profile() != *change.profile())
    return false;
  return type() == ADD || pre_update_label_ == change.pre_update_label();
}

AutofillCreditCardChange::AutofillCreditCardChange(Type t,
                                                   string16 k,
                                                   const CreditCard* card)
    : GenericAutofillChange<string16>(t, k), credit_card_(card) {}

AutofillCreditCardChange::~AutofillCreditCardChange() {}
