// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/sync_driver/pref_names.h"
#include "policy/policy_constants.h"

namespace browser_sync {

SyncPolicyHandler::SyncPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kSyncDisabled,
                                        base::Value::TYPE_BOOLEAN) {}

SyncPolicyHandler::~SyncPolicyHandler() {
}

void SyncPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  bool disable_sync;
  if (value && value->GetAsBoolean(&disable_sync) && disable_sync)
    prefs->SetValue(sync_driver::prefs::kSyncManaged, value->DeepCopy());
}

}  // namespace browser_sync
