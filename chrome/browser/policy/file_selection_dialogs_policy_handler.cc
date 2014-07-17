// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_selection_dialogs_policy_handler.h"

#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

FileSelectionDialogsPolicyHandler::FileSelectionDialogsPolicyHandler()
    : TypeCheckingPolicyHandler(key::kAllowFileSelectionDialogs,
                                base::Value::TYPE_BOOLEAN) {}

FileSelectionDialogsPolicyHandler::~FileSelectionDialogsPolicyHandler() {}

void FileSelectionDialogsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  bool allow_dialogs;
  const base::Value* value = policies.GetValue(policy_name());
  if (value && value->GetAsBoolean(&allow_dialogs)) {
    prefs->SetBoolean(prefs::kAllowFileSelectionDialogs, allow_dialogs);
    // Disallow selecting the download location if file dialogs are disabled.
    if (!allow_dialogs)
      prefs->SetBoolean(prefs::kPromptForDownload, false);
  }
}

}  // namespace policy
