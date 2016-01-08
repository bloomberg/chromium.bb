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
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

namespace {

// Maximum time to wait before forcing a decision.  Note that download time for
// state key buckets can be non-negligible, especially on 2G connections.
const int kSafeguardTimeoutSeconds = 60;

// Returns the int value of the |switch_name| argument, clamped to the [0, 62]
// interval. Returns 0 if the argument doesn't exist or isn't an int value.
int GetSanitizedArg(const std::string& switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
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

// Checks whether the device is yet to be set up by the first user in its
// lifetime. After first setup, the activation date gets stored in the R/W VPD,
// the absence of this key signals the device is factory-fresh. The requirement
// for the machine serial number to be present as well is a sanity-check to
// ensure that the VPD has actually been read successfully.
bool IsFirstDeviceSetup() {
  std::string activate_date;
  return !system::StatisticsProvider::GetInstance()->HasMachineStatistic(
             system::kActivateDateKey) &&
         !policy::DeviceCloudPolicyManagerChromeOS::GetMachineID().empty();
}

}  // namespace

const char AutoEnrollmentController::kForcedReEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kForcedReEnrollmentNever[] = "never";
const char AutoEnrollmentController::kForcedReEnrollmentOfficialBuild[] =
    "official";

AutoEnrollmentController::Mode AutoEnrollmentController::GetMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways) {
    return MODE_FORCED_RE_ENROLLMENT;
  } else if (command_line_mode.empty() ||
             command_line_mode == kForcedReEnrollmentOfficialBuild) {
#if defined(OFFICIAL_BUILD)
    std::string firmware_type;
    const bool non_chrome_firmware =
        system::StatisticsProvider::GetInstance()->GetMachineStatistic(
            system::kFirmwareTypeKey, &firmware_type) &&
        firmware_type == system::kFirmwareTypeValueNonchrome;
    return non_chrome_firmware ? MODE_NONE : MODE_FORCED_RE_ENROLLMENT;
#else
    return MODE_NONE;
#endif
  } else if (command_line_mode == kForcedReEnrollmentNever) {
    return MODE_NONE;
  }

  LOG(FATAL) << "Unknown auto-enrollment mode " << command_line_mode;
  return MODE_NONE;
}

AutoEnrollmentController::AutoEnrollmentController()
    : state_(policy::AUTO_ENROLLMENT_STATE_IDLE),
      safeguard_timer_(false, false),
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
  // 4. This is the first boot ever, so re-enrollment checks are pointless. This
  //    also enables factories to start full guest sessions for testing, see
  //    http://crbug.com/397354 for more context.

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kDisableGaiaServices) ||
      (!command_line->HasSwitch(
           chromeos::switches::kEnterpriseEnrollmentInitialModulus) &&
       !command_line->HasSwitch(
           chromeos::switches::kEnterpriseEnrollmentModulusLimit)) ||
      GetMode() == MODE_NONE ||
      IsFirstDeviceSetup()) {
    LOG(WARNING) << "Auto-enrollment disabled.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }

  // If a client is being created or already existing, bail out.
  if (client_start_weak_factory_.HasWeakPtrs() || client_) {
    LOG(ERROR) << "Auto-enrollment client is already running.";
    return;
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSafeguardTimeoutSeconds),
      base::Bind(&AutoEnrollmentController::Timeout, base::Unretained(this)));

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

  safeguard_timer_.Stop();
}

void AutoEnrollmentController::Retry() {
  if (client_)
    client_->Retry();
  else
    Start();
}

scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case DeviceSettingsService::OWNERSHIP_NONE: {
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetStateKeysBroker()
          ->RequestStateKeys(
              base::Bind(&AutoEnrollmentController::StartClient,
                         client_start_weak_factory_.GetWeakPtr()));
      break;
    }
    case DeviceSettingsService::OWNERSHIP_TAKEN: {
      // This is part of normal operation.  Logging as "WARNING" nevertheless to
      // make sure it's preserved in the logs.
      LOG(WARNING) << "Device already owned, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      break;
    }
    case DeviceSettingsService::OWNERSHIP_UNKNOWN: {
      LOG(ERROR) << "Ownership unknown, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      break;
    }
  }
}

void AutoEnrollmentController::StartClient(
    const std::vector<std::string>& state_keys) {
  if (state_keys.empty()) {
    LOG(ERROR) << "No state keys available!";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }

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
                  "clamping to the limit.";
    power_initial = power_limit;
  }

  client_.reset(new policy::AutoEnrollmentClient(
      base::Bind(&AutoEnrollmentController::UpdateState,
                 base::Unretained(this)),
      service,
      g_browser_process->local_state(),
      g_browser_process->system_request_context(),
      state_keys.front(),
      power_initial,
      power_limit));

  VLOG(1) << "Starting auto-enrollment client.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(
    policy::AutoEnrollmentState new_state) {
  // This is part of normal operation.  Logging as "WARNING" nevertheless to
  // make sure it's preserved in the logs.
  LOG(WARNING) << "New auto-enrollment state: " << new_state;
  state_ = new_state;

  // Stop the safeguard timer once a result comes in.
  switch (state_) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
      break;
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      safeguard_timer_.Stop();
      break;
  }

  progress_callbacks_.Notify(state_);
}

void AutoEnrollmentController::Timeout() {
  // TODO(mnissler): Add UMA to track results of auto-enrollment checks.
  if (client_start_weak_factory_.HasWeakPtrs()) {
    // If the callbacks to check ownership status or state keys are still
    // pending, there's a bug in the code running on the device. No use in
    // retrying anything, need to fix that bug.
    LOG(ERROR) << "Failed to start auto-enrollment check, fix the code!";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  } else {
    // If the AutoEnrollmentClient didn't manage to come back with a result in
    // time, blame it on the network. This can actually happen in some cases,
    // for example when the server just doesn't reply and keeps the connection
    // open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
  }

  // Reset state.
  Cancel();
}

}  // namespace chromeos
