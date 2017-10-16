// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tpm_firmware_update.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace chromeos {
namespace tpm_firmware_update {

namespace {

// Settings dictionary key constants.
constexpr char kSettingsKeyAllowPowerwash[] = "allow-user-initiated-powerwash";

// The presence of this flag file indicates that a firmware update is available.
constexpr base::FilePath::CharType kTPMFirmwareUpdateAvailableFlagFile[] =
    FILE_PATH_LITERAL("/run/tpm_firmware_update_available");

// Checks whether |kSettingsKeyAllowPowerwash| is set to true in |settings|.
bool SettingsAllowUpdateViaPowerwash(const base::Value* settings) {
  if (!settings)
    return false;

  const base::Value* const allow_powerwash = settings->FindKeyOfType(
      kSettingsKeyAllowPowerwash, base::Value::Type::BOOLEAN);
  return allow_powerwash && allow_powerwash->GetBool();
}

}  // namespace

std::unique_ptr<base::DictionaryValue> DecodeSettingsProto(
    const enterprise_management::TPMFirmwareUpdateSettingsProto& settings) {
  std::unique_ptr<base::DictionaryValue> result =
      base::MakeUnique<base::DictionaryValue>();

  if (settings.has_allow_user_initiated_powerwash()) {
    result->SetPath({kSettingsKeyAllowPowerwash},
                    base::Value(settings.allow_user_initiated_powerwash()));
  }

  return result;
}

void ShouldOfferUpdateViaPowerwash(base::OnceCallback<void(bool)> completion) {
  // Wrap |completion| in a RepeatingCallback. This is necessary to cater to the
  // somewhat awkward PrepareTrustedValues interface, which for some return
  // values invokes the callback passed to it, and for others requires the code
  // here to do so.
  base::RepeatingCallback<void(bool)> callback(
      base::AdaptCallbackForRepeating(std::move(completion)));

  if (!base::FeatureList::IsEnabled(features::kTPMFirmwareUpdate)) {
    callback.Run(false);
    return;
  }

  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged()) {
    // For enterprise-managed devices, always honor the device setting.
    CrosSettings* const cros_settings = CrosSettings::Get();
    switch (cros_settings->PrepareTrustedValues(
        base::Bind(&ShouldOfferUpdateViaPowerwash, callback))) {
      case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
        // Retry happens via the callback registered above.
        return;
      case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
        // No device settings? Default to disallow.
        callback.Run(false);
        return;
      case CrosSettingsProvider::TRUSTED:
        // Setting is present and trusted so respect its value.
        if (!SettingsAllowUpdateViaPowerwash(
                cros_settings->GetPref(kTPMFirmwareUpdateSettings))) {
          callback.Run(false);
          return;
        }
        break;
    }
  } else {
    // Consumer device or still in OOBE. If FRE is required, enterprise
    // enrollment might still be pending, in which case powerwash is disallowed
    // until FRE determines that the device is not remotely managed or it does
    // get enrolled and the admin allows TPM firmware update via powerwash.
    const AutoEnrollmentController::FRERequirement requirement =
        AutoEnrollmentController::GetFRERequirement();
    if (requirement == AutoEnrollmentController::EXPLICITLY_REQUIRED) {
      callback.Run(false);
      return;
    }
  }

  // OK to offer TPM firmware update via powerwash to the user. Last thing to
  // check is whether there actually is a pending update.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&base::PathExists,
                 base::FilePath(kTPMFirmwareUpdateAvailableFlagFile)),
      callback);
}

}  // namespace tpm_firmware_update
}  // namespace chromeos
