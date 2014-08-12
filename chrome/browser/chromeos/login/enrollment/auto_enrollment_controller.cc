// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

namespace {

// Returns the int value of the |switch_name| argument, clamped to the [0, 62]
// interval. Returns 0 if the argument doesn't exist or isn't an int value.
int GetSanitizedArg(const std::string& switch_name) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name))
    return 0;
  std::string value = command_line->GetSwitchValueASCII(switch_name);
  int int_value;
  if (!base::StringToInt(value, &int_value)) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" is not a valid int. "
               << "Defaulting to 0.";
    return 0;
  }
  if (int_value < 0) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be negative. "
               << "Using 0";
    return 0;
  }
  if (int_value > policy::AutoEnrollmentClient::kMaximumPower) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be greater than "
               << policy::AutoEnrollmentClient::kMaximumPower << ". Using "
               << policy::AutoEnrollmentClient::kMaximumPower;
    return policy::AutoEnrollmentClient::kMaximumPower;
  }
  return int_value;
}

}  // namespace

const char AutoEnrollmentController::kForcedReEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kForcedReEnrollmentLegacy[] = "legacy";
const char AutoEnrollmentController::kForcedReEnrollmentNever[] = "never";
const char AutoEnrollmentController::kForcedReEnrollmentOfficialBuild[] =
    "official";

AutoEnrollmentController::Mode AutoEnrollmentController::GetMode() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kEnterpriseEnableForcedReEnrollment))
    return MODE_LEGACY_AUTO_ENROLLMENT;

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways) {
    return MODE_FORCED_RE_ENROLLMENT;
  } else if (command_line_mode.empty() ||
             command_line_mode == kForcedReEnrollmentOfficialBuild) {
#if defined(OFFICIAL_BUILD)
    return MODE_FORCED_RE_ENROLLMENT;
#else
    return MODE_NONE;
#endif
  } else if (command_line_mode == kForcedReEnrollmentLegacy) {
    return MODE_LEGACY_AUTO_ENROLLMENT;
  }

  return MODE_NONE;
}

AutoEnrollmentController::AutoEnrollmentController()
    : state_(policy::AUTO_ENROLLMENT_STATE_IDLE),
      client_start_weak_factory_(this) {}

AutoEnrollmentController::~AutoEnrollmentController() {}

void AutoEnrollmentController::Start() {
  // This method is called at the point in the OOBE/login flow at which the
  // auto-enrollment check can start. This happens either after the EULA is
  // accepted, or right after a reboot if the EULA has already been accepted.

  // Do not communicate auto-enrollment data to the server if
  // 1. we are running telemetry tests.
  // 2. modulus configuration is not present.
  // 3. Auto-enrollment is disabled via the command line.

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kDisableGaiaServices) ||
      (!command_line->HasSwitch(
           chromeos::switches::kEnterpriseEnrollmentInitialModulus) &&
       !command_line->HasSwitch(
           chromeos::switches::kEnterpriseEnrollmentModulusLimit)) ||
      GetMode() == MODE_NONE) {
    VLOG(1) << "Auto-enrollment disabled.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }

  // If a client is being created or already existing, bail out.
  if (client_start_weak_factory_.HasWeakPtrs() || client_)
    return;

  // Start by checking if the device has already been owned.
  UpdateState(policy::AUTO_ENROLLMENT_STATE_PENDING);
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&AutoEnrollmentController::OnOwnershipStatusCheckDone,
                 client_start_weak_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::Cancel() {
  if (client_) {
    // Cancelling the |client_| allows it to determine whether
    // its protocol finished before login was complete.
    client_.release()->CancelAndDeleteSoon();
  }

  // Make sure to nuke pending |client_| start sequences.
  client_start_weak_factory_.InvalidateWeakPtrs();
}

void AutoEnrollmentController::Retry() {
  if (client_)
    client_->Retry();
}

scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

bool AutoEnrollmentController::ShouldEnrollSilently() {
  return state_ == policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT &&
         GetMode() == MODE_LEGACY_AUTO_ENROLLMENT;
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  if (status != DeviceSettingsService::OWNERSHIP_NONE) {
    // The device is already owned. No need for auto-enrollment checks.
    VLOG(1) << "Device already owned, skipping auto-enrollment check";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }

  // Make sure state keys are available.
  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetStateKeysBroker()
      ->RequestStateKeys(base::Bind(&AutoEnrollmentController::StartClient,
                                    client_start_weak_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartClient(
    const std::vector<std::string>& state_keys) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceManagementService* service =
      connector->device_management_service();
  service->ScheduleInitialization(0);

  int power_initial = GetSanitizedArg(
      chromeos::switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit = GetSanitizedArg(
      chromeos::switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
               << "clamping to the limit.";
    power_initial = power_limit;
  }

  bool retrieve_device_state = false;
  std::string device_id;
  if (GetMode() == MODE_FORCED_RE_ENROLLMENT) {
    retrieve_device_state = true;
    device_id = state_keys.empty() ? std::string() : state_keys.front();
  } else {
    device_id = policy::DeviceCloudPolicyManagerChromeOS::GetMachineID();
  }

  client_.reset(new policy::AutoEnrollmentClient(
      base::Bind(&AutoEnrollmentController::UpdateState,
                 base::Unretained(this)),
      service,
      g_browser_process->local_state(),
      g_browser_process->system_request_context(),
      device_id,
      retrieve_device_state,
      power_initial,
      power_limit));

  VLOG(1) << "Starting auto-enrollment client.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(
    policy::AutoEnrollmentState new_state) {
  VLOG(1) << "New state: " << new_state << ".";
  state_ = new_state;
  progress_callbacks_.Notify(state_);
}

}  // namespace chromeos
