// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/autofill_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

AutofillPolicyHandler::AutofillPolicyHandler()
    : TypeCheckingPolicyHandler(key::kAutoFillEnabled,
                                base::Value::TYPE_BOOLEAN) {}

AutofillPolicyHandler::~AutofillPolicyHandler() {
}

void AutofillPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  bool auto_fill_enabled;
  if (value && value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled)
    prefs->SetBoolean(autofill::prefs::kAutofillEnabled, false);
}

}  // namespace policy
