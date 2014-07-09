// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_dir_policy_handler.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

namespace {
#if defined(OS_CHROMEOS)
const char* kDriveNamePolicyVariableName = "${google_drive}";

// Drive root folder relative to its mount point.
const base::FilePath::CharType* kRootRelativeToDriveMount =
    FILE_PATH_LITERAL("root");
#endif
}  // namespace

DownloadDirPolicyHandler::DownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kDownloadDirectory,
                                base::Value::TYPE_STRING) {}

DownloadDirPolicyHandler::~DownloadDirPolicyHandler() {}

bool DownloadDirPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

#if defined(OS_CHROMEOS)
  // Download directory can only be set as a user policy. If it is set through
  // platform policy for a chromeos=1 build, ignore it.
  if (value &&
      policies.Get(policy_name())->scope != policy::POLICY_SCOPE_USER) {
    errors->AddError(policy_name(), IDS_POLICY_SCOPE_ERROR);
    return false;
  }
#endif

  return true;
}

void DownloadDirPolicyHandler::ApplyPolicySettingsWithParameters(
    const policy::PolicyMap& policies,
    const policy::PolicyHandlerParameters& parameters,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  base::FilePath::StringType string_value;
  if (!value || !value->GetAsString(&string_value))
    return;

  base::FilePath::StringType expanded_value;
#if defined(OS_CHROMEOS)
  bool download_to_drive = false;
  // TODO(kaliamoorthi): Clean up policy::path_parser and fold this code
  // into it. http://crbug.com/352627
  size_t position = string_value.find(kDriveNamePolicyVariableName);
  if (position != base::FilePath::StringType::npos) {
    base::FilePath::StringType google_drive_root;
    if (!parameters.user_id_hash.empty()) {
      google_drive_root = drive::util::GetDriveMountPointPathForUserIdHash(
                              parameters.user_id_hash)
                              .Append(kRootRelativeToDriveMount)
                              .value();
      download_to_drive = true;
    }
    expanded_value = string_value.replace(
        position,
        base::FilePath::StringType(kDriveNamePolicyVariableName).length(),
        google_drive_root);
  } else {
    expanded_value = string_value;
  }
#else
  expanded_value = policy::path_parser::ExpandPathVariables(string_value);
#endif
  // Make sure the path isn't empty, since that will point to an undefined
  // location; the default location is used instead in that case.
  // This is checked after path expansion because a non-empty policy value can
  // lead to an empty path value after expansion (e.g. "\"\"").
  if (expanded_value.empty())
    expanded_value = DownloadPrefs::GetDefaultDownloadDirectory().value();
  prefs->SetValue(prefs::kDownloadDefaultDirectory,
                  new base::StringValue(expanded_value));

  // If the policy is mandatory, prompt for download should be disabled.
  // Otherwise, it would enable a user to bypass the mandatory policy.
  if (policies.Get(policy_name())->level == policy::POLICY_LEVEL_MANDATORY) {
    prefs->SetValue(prefs::kPromptForDownload,
                    new base::FundamentalValue(false));
#if defined(OS_CHROMEOS)
    if (download_to_drive) {
      prefs->SetValue(prefs::kDisableDrive,
                      new base::FundamentalValue(false));
    }
#endif
  }
}
