// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tpm_firmware_update.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace chromeos {
namespace tpm_firmware_update {

namespace {

// Checks whether |kSettingsKeyAllowPowerwash| is set to true in |settings|.
bool SettingsAllowUpdateViaPowerwash(const base::Value* settings) {
  if (!settings)
    return false;

  const base::Value* const allow_powerwash = settings->FindKeyOfType(
      kSettingsKeyAllowPowerwash, base::Value::Type::BOOLEAN);
  return allow_powerwash && allow_powerwash->GetBool();
}

}  // namespace

const char kSettingsKeyAllowPowerwash[] = "allow-user-initiated-powerwash";

std::unique_ptr<base::DictionaryValue> DecodeSettingsProto(
    const enterprise_management::TPMFirmwareUpdateSettingsProto& settings) {
  std::unique_ptr<base::DictionaryValue> result =
      std::make_unique<base::DictionaryValue>();

  if (settings.has_allow_user_initiated_powerwash()) {
    result->SetKey(kSettingsKeyAllowPowerwash,
                   base::Value(settings.allow_user_initiated_powerwash()));
  }

  return result;
}

// AvailabilityChecker tracks TPM firmware update availability information
// exposed by the system via the /run/tpm_firmware_update file. There are three
// states:
//  1. The file isn't present - availability check is still pending.
//  2. The file is present, but empty - no update available.
//  3. The file is present, non-empty - update binary path is in the file.
//
// AvailabilityChecker employs a FilePathWatcher to watch the file and hides
// away all the gory threading details.
class AvailabilityChecker {
 public:
  using ResponseCallback = base::OnceCallback<void(bool)>;

  ~AvailabilityChecker() { Cancel(); }

  static void Start(ResponseCallback callback, base::TimeDelta timeout) {
    // Schedule a task to run when the timeout expires. The task also owns
    // |checker| and thus takes care of eventual deletion.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AvailabilityChecker::OnTimeout,
            std::make_unique<AvailabilityChecker>(std::move(callback))),
        timeout);
  }

  // Don't call this directly, but use Start().
  explicit AvailabilityChecker(ResponseCallback callback)
      : callback_(std::move(callback)),
        background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
        watcher_(new base::FilePathWatcher()),
        weak_ptr_factory_(this) {
    auto watch_callback = base::BindRepeating(
        &AvailabilityChecker::OnFilePathChanged,
        base::SequencedTaskRunnerHandle::Get(), weak_ptr_factory_.GetWeakPtr());
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AvailabilityChecker::StartOnBackgroundThread,
                                  watcher_.get(), watch_callback));
  }

 private:
  static base::FilePath GetUpdateLocationFilePath() {
    base::FilePath update_location_file;
    CHECK(PathService::Get(chrome::FILE_CHROME_OS_TPM_FIRMWARE_UPDATE_LOCATION,
                           &update_location_file));
    return update_location_file;
  }

  static bool IsUpdateAvailable() {
    int64_t size;
    return base::GetFileSize(GetUpdateLocationFilePath(), &size) && size;
  }

  static void StartOnBackgroundThread(
      base::FilePathWatcher* watcher,
      base::FilePathWatcher::Callback watch_callback) {
    watcher->Watch(GetUpdateLocationFilePath(), false /* recursive */,
                   watch_callback);
    watch_callback.Run(base::FilePath(), false /* error */);
  }

  static void OnFilePathChanged(
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      base::WeakPtr<AvailabilityChecker> checker,
      const base::FilePath& target,
      bool error) {
    bool available = IsUpdateAvailable();
    if (available || error) {
      origin_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&AvailabilityChecker::Resolve, checker, available));
    }
  }

  void Resolve(bool available) {
    Cancel();
    if (callback_) {
      std::move(callback_).Run(available);
    }
  }

  void Cancel() {
    // Neutralize further callbacks from |watcher_| or due to timeout.
    weak_ptr_factory_.InvalidateWeakPtrs();
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(watcher_));
  }

  void OnTimeout() {
    // If |callback_| hasn't been triggered when the timeout task fires, perform
    // a last check and wire the result into a |callback_| execution to make
    // sure a result is delivered in all cases. Note that |OnTimeout()| gets run
    // via a callback that owns |this|, so the object will be destructed after
    // this function terminates. Thus, the final check needs to run independent
    // of |this| and takes |callback_| ownership.
    if (callback_) {
      base::PostTaskAndReplyWithResult(
          background_task_runner_.get(), FROM_HERE,
          base::BindOnce(&AvailabilityChecker::IsUpdateAvailable),
          std::move(callback_));
    }
  }

  ResponseCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  std::unique_ptr<base::FilePathWatcher> watcher_;
  base::WeakPtrFactory<AvailabilityChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AvailabilityChecker);
};

void ShouldOfferUpdateViaPowerwash(
    base::OnceCallback<void(bool)> completion,
    base::TimeDelta timeout) {
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
        base::Bind(&ShouldOfferUpdateViaPowerwash, callback, timeout))) {
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
  AvailabilityChecker::Start(callback, timeout);
}

}  // namespace tpm_firmware_update
}  // namespace chromeos
