// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace payments {

const char kPaymentsFirstTransactionCompleted[] =
    "payments.first_transaction_completed";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kPaymentsFirstTransactionCompleted, false);
}

}  // namespace payments
