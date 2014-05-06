// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"

namespace policy {
class ServerBackedStateKeysBroker;
}

namespace chromeos {

// Drives the auto-enrollment check, running an AutoEnrollmentClient if
// appropriate to make a decision.
class AutoEnrollmentController {
 public:
  typedef base::CallbackList<void(policy::AutoEnrollmentState)>
      ProgressCallbackList;

  // Parameter values for the kEnterpriseEnableForcedReEnrollment flag.
  static const char kForcedReEnrollmentAlways[];
  static const char kForcedReEnrollmentLegacy[];
  static const char kForcedReEnrollmentNever[];
  static const char kForcedReEnrollmentOfficialBuild[];

  // Auto-enrollment modes.
  enum Mode {
    // No automatic enrollment.
    MODE_NONE,
    // Legacy auto-enrollment.
    MODE_LEGACY_AUTO_ENROLLMENT,
    // Forced re-enrollment.
    MODE_FORCED_RE_ENROLLMENT,
  };

  // Gets the auto-enrollment mode based on command-line flags and official
  // build status.
  static Mode GetMode();

  AutoEnrollmentController();
  ~AutoEnrollmentController();

  // Starts the auto-enrollment check.
  void Start();

  // Stops any pending auto-enrollment checking.
  void Cancel();

  // Retry checking.
  void Retry();

  // Registers a callback to invoke on state changes.
  scoped_ptr<ProgressCallbackList::Subscription> RegisterProgressCallback(
      const ProgressCallbackList::CallbackType& callback);

  // Checks whether legacy auto-enrollment should be performed.
  bool ShouldEnrollSilently();

  policy::AutoEnrollmentState state() const { return state_; }

 private:
  // Callback for the ownership status check.
  void OnOwnershipStatusCheckDone(
      DeviceSettingsService::OwnershipStatus status);

  // Starts the auto-enrollment client.
  void StartClient(const std::vector<std::string>& state_keys);

  // Sets |state_| and notifies |progress_callbacks_|.
  void UpdateState(policy::AutoEnrollmentState state);

  policy::AutoEnrollmentState state_;
  ProgressCallbackList progress_callbacks_;

  base::WeakPtrFactory<AutoEnrollmentController> client_start_weak_factory_;

  scoped_ptr<policy::AutoEnrollmentClient> client_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
