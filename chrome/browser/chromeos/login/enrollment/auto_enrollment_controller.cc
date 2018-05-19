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
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/system/factory_ping_embargo_check.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {

namespace {

// Maximum number of bits of the identifer hash to send during initial
// enrollment check.
const int kInitialEnrollmentModulusPowerLimit = 6;

// Maximum time to wait before forcing a decision.  Note that download time for
// state key buckets can be non-negligible, especially on 2G connections.
const int kSafeguardTimeoutSeconds = 90;

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

std::string FRERequirementToString(
    AutoEnrollmentController::FRERequirement requirement) {
  using FRERequirement = AutoEnrollmentController::FRERequirement;
  switch (requirement) {
    case FRERequirement::kRequired:
      return "Auto-enrollment required.";
    case FRERequirement::kNotRequired:
      return "Auto-enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Auto-enrollment required: flag in VPD.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Auto-enrollment disabled: flag in VPD.";
  }

  NOTREACHED();
  return std::string();
}

// Returns true if this is an official build and the device has chrome firmware.
bool IsOfficialChrome() {
#if defined(OFFICIAL_BUILD)
  std::string firmware_type;
  const bool non_chrome_firmware =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kFirmwareTypeKey, &firmware_type) &&
      firmware_type == system::kFirmwareTypeValueNonchrome;
  return !non_chrome_firmware;
#else
  return false;
#endif
}

// Schedules immediate initialization of the |DeviceManagementService| and
// returns it.
policy::DeviceManagementService* InitializeAndGetDeviceManagementService() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceManagementService* service =
      connector->device_management_service();
  service->ScheduleInitialization(0);
  return service;
}

}  // namespace

const char AutoEnrollmentController::kForcedReEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kForcedReEnrollmentNever[] = "never";
const char AutoEnrollmentController::kForcedReEnrollmentOfficialBuild[] =
    "official";

const char AutoEnrollmentController::kInitialEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kInitialEnrollmentNever[] = "never";
const char AutoEnrollmentController::kInitialEnrollmentOfficialBuild[] =
    "official";

// static
bool AutoEnrollmentController::IsFREEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kForcedReEnrollmentOfficialBuild) {
    return IsOfficialChrome();
  }

  if (command_line_mode == kForcedReEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown auto-enrollment mode for FRE " << command_line_mode;
  return false;
}

// static
bool AutoEnrollmentController::IsInitialEnrollmentEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kEnterpriseEnableInitialEnrollment))
    return false;

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableInitialEnrollment);
  if (command_line_mode == kInitialEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kInitialEnrollmentOfficialBuild) {
    return IsOfficialChrome();
  }

  if (command_line_mode == kInitialEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown auto-enrollment mode for initial enrollment "
             << command_line_mode;
  return false;
}

// static
bool AutoEnrollmentController::IsEnabled() {
  return IsFREEnabled() || IsInitialEnrollmentEnabled();
}

// static
AutoEnrollmentController::FRERequirement
AutoEnrollmentController::GetFRERequirement() {
  std::string check_enrollment_value;
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  bool fre_flag_found = provider->GetMachineStatistic(
      system::kCheckEnrollmentKey, &check_enrollment_value);

  if (fre_flag_found) {
    if (check_enrollment_value == "0")
      return FRERequirement::kExplicitlyNotRequired;
    if (check_enrollment_value == "1")
      return FRERequirement::kExplicitlyRequired;
  }
  if (!provider->GetMachineStatistic(system::kActivateDateKey, nullptr) &&
      !provider->GetEnterpriseMachineID().empty()) {
    return FRERequirement::kNotRequired;
  }
  return FRERequirement::kRequired;
}

// static
AutoEnrollmentController::InitialEnrollmentRequirement
AutoEnrollmentController::GetInitialEnrollmentRequirement() {
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  system::FactoryPingEmbargoState embargo_state =
      system::GetFactoryPingEmbargoState(provider);
  if (embargo_state == system::FactoryPingEmbargoState::kInvalid) {
    LOG(WARNING)
        << "Skip Initial Enrollment Check due to invalid embargo date.";
    // TODO(pmarko): UMA Stat.
    return InitialEnrollmentRequirement::kNotRequired;
  }
  if (embargo_state == system::FactoryPingEmbargoState::kNotPassed) {
    VLOG(1) << "Skip Initial Enrollment Check due to not-passed embargo date.";
    return InitialEnrollmentRequirement::kNotRequired;
  }

  if (provider->GetEnterpriseMachineID().empty()) {
    LOG(WARNING)
        << "Skip Initial Enrollment Check due to missing serial number.";
    return InitialEnrollmentRequirement::kNotRequired;
  }

  std::string rlz_brand_code;
  const bool rlz_brand_code_found =
      provider->GetMachineStatistic(system::kRlzBrandCodeKey, &rlz_brand_code);
  if (!rlz_brand_code_found || rlz_brand_code.empty()) {
    LOG(WARNING) << "Skip Initial Enrollment Check due to missing brand code.";
    return InitialEnrollmentRequirement::kNotRequired;
  }

  return InitialEnrollmentRequirement::kRequired;
}

AutoEnrollmentController::AutoEnrollmentController() {}

AutoEnrollmentController::~AutoEnrollmentController() {}

void AutoEnrollmentController::Start() {
  switch (state_) {
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
      // Abort re-start if the check is still running.
      return;
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
      // Abort re-start when there's already a final decision.
      return;

    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      // Continue (re-)start.
      break;
  }

  // If a client is being created or already existing, bail out.
  if (client_start_weak_factory_.HasWeakPtrs() || client_) {
    LOG(ERROR) << "Auto-enrollment client is already running.";
    return;
  }

  DetermineAutoEnrollmentCheckType();
  if (auto_enrollment_check_type_ == AutoEnrollmentCheckType::kNone) {
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromSeconds(kSafeguardTimeoutSeconds),
                         base::Bind(&AutoEnrollmentController::Timeout,
                                    weak_ptr_factory_.GetWeakPtr()));

  // Start by checking if the device has already been owned.
  UpdateState(policy::AUTO_ENROLLMENT_STATE_PENDING);
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&AutoEnrollmentController::OnOwnershipStatusCheckDone,
                 client_start_weak_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::Retry() {
  if (client_)
    client_->Retry();
  else
    Start();
}

std::unique_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

void AutoEnrollmentController::DetermineAutoEnrollmentCheckType() {
  // Skip everything if neither FRE nor Initial Enrollment are enabled.
  if (!IsEnabled()) {
    VLOG(1) << "Auto-enrollment disabled";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
    return;
  }

  // Skip everything if GAIA is disabled.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGaiaServices)) {
    VLOG(1) << "Auto-enrollment disabled: command line (gaia).";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
    return;
  }

  // Skip everything if the device was in consumer mode previously.
  fre_requirement_ = GetFRERequirement();
  VLOG(1) << FRERequirementToString(fre_requirement_);
  if (fre_requirement_ == FRERequirement::kExplicitlyNotRequired) {
    VLOG(1) << "Auto-enrollment disabled: VPD";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
    return;
  }

  if (ShouldDoFRECheck(command_line, fre_requirement_)) {
    // FRE has precedence over Initial Enrollment.
    VLOG(1) << "Proceeding with FRE";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kFRE;
    return;
  }

  if (ShouldDoInitialEnrollmentCheck()) {
    VLOG(1) << "Proceeding with Initial Enrollment";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kInitialEnrollment;
    return;
  }

  auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
}

// static
bool AutoEnrollmentController::ShouldDoFRECheck(
    base::CommandLine* command_line,
    FRERequirement fre_requirement) {
  // Skip FRE check if modulus configuration is not present.
  if (!command_line->HasSwitch(switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(switches::kEnterpriseEnrollmentModulusLimit)) {
    VLOG(1) << "FRE disabled: command line (config)";
    return false;
  }

  // Skip FRE check if it is not enabled by command-line switches.
  if (!IsFREEnabled()) {
    VLOG(1) << "FRE disabled";
    return false;
  }

  // Skip FRE check if it is not required according to the device state.
  if (fre_requirement == FRERequirement::kNotRequired)
    return false;

  return true;
}

// static
bool AutoEnrollmentController::ShouldDoInitialEnrollmentCheck() {
  // Skip Initial Enrollment check if it is not enabled according to
  // command-line flags.
  if (!IsInitialEnrollmentEnabled())
    return false;

  // Skip Initial Enrollment check if it is not required according to the
  // device state.
  if (GetInitialEnrollmentRequirement() ==
      InitialEnrollmentRequirement::kNotRequired)
    return false;

  return true;
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case DeviceSettingsService::OWNERSHIP_NONE:
      switch (auto_enrollment_check_type_) {
        case AutoEnrollmentCheckType::kFRE:
          // For FRE, request state keys first.
          g_browser_process->platform_part()
              ->browser_policy_connector_chromeos()
              ->GetStateKeysBroker()
              ->RequestStateKeys(
                  base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                                 client_start_weak_factory_.GetWeakPtr()));
          break;
        case AutoEnrollmentCheckType::kInitialEnrollment:
          StartClientForInitialEnrollment();
          break;
        case AutoEnrollmentCheckType::kNone:
          // The ownership check is only triggered if
          // |auto_enrollment_check_type_| indicates that an auto-enrollment
          // check should be done.
          NOTREACHED();
          break;
      }
      return;
    case DeviceSettingsService::OWNERSHIP_TAKEN:
      VLOG(1) << "Device already owned, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      return;
    case DeviceSettingsService::OWNERSHIP_UNKNOWN:
      LOG(ERROR) << "Ownership unknown, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      return;
  }
}

void AutoEnrollmentController::StartClientForFRE(
    const std::vector<std::string>& state_keys) {
  if (state_keys.empty()) {
    LOG(ERROR) << "No state keys available";
    if (fre_requirement_ == FRERequirement::kExplicitlyRequired) {
      // Retry to fetch the state keys. For devices where FRE is required to be
      // checked, we can't proceed with empty state keys.
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetStateKeysBroker()
          ->RequestStateKeys(
              base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                             client_start_weak_factory_.GetWeakPtr()));
    } else {
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    }
    return;
  }

  policy::DeviceManagementService* service =
      InitializeAndGetDeviceManagementService();

  int power_initial =
      GetSanitizedArg(switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit =
      GetSanitizedArg(switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
                  "clamping to the limit.";
    power_initial = power_limit;
  }

  client_ = policy::AutoEnrollmentClient::CreateForFRE(
      base::Bind(&AutoEnrollmentController::UpdateState,
                 weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(),
      g_browser_process->system_request_context(), state_keys.front(),
      power_initial, power_limit);

  VLOG(1) << "Starting auto-enrollment client for FRE.";
  client_->Start();
}

void AutoEnrollmentController::StartClientForInitialEnrollment() {
  policy::DeviceManagementService* service =
      InitializeAndGetDeviceManagementService();

  // Initial Enrollment does not transfer any data in the initial exchange, and
  // supports uploading up to |kInitialEnrollmentModulusPowerLimit| bits of the
  // identifier hash.
  const int power_initial = 0;
  const int power_limit = kInitialEnrollmentModulusPowerLimit;

  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  std::string serial_number = provider->GetEnterpriseMachineID();
  std::string rlz_brand_code;
  const bool rlz_brand_code_found =
      provider->GetMachineStatistic(system::kRlzBrandCodeKey, &rlz_brand_code);
  // The initial enrollment check should not be started if the serial number or
  // brand code are missing. This is ensured in
  // |GetInitialEnrollmentRequirement|.
  CHECK(!serial_number.empty() && rlz_brand_code_found &&
        !rlz_brand_code.empty());

  client_ = policy::AutoEnrollmentClient::CreateForInitialEnrollment(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(),
      g_browser_process->system_request_context(), serial_number,
      rlz_brand_code, power_initial, power_limit);

  VLOG(1) << "Starting auto-enrollment client for Initial Enrollment.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(
    policy::AutoEnrollmentState new_state) {
  VLOG(1) << "New auto-enrollment state: " << new_state;
  state_ = new_state;

  // Stop the safeguard timer once a result comes in.
  switch (state_) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
      break;
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      safeguard_timer_.Stop();
      break;
  }

  if (state_ == policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT) {
    StartRemoveFirmwareManagementParameters();
  } else {
    progress_callbacks_.Notify(state_);
  }
}

void AutoEnrollmentController::StartRemoveFirmwareManagementParameters() {
  DCHECK_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);

  cryptohome::RemoveFirmwareManagementParametersRequest request;
  DBusThreadManager::Get()
      ->GetCryptohomeClient()
      ->RemoveFirmwareManagementParametersFromTpm(
          request,
          base::BindOnce(
              &AutoEnrollmentController::OnFirmwareManagementParametersRemoved,
              weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnFirmwareManagementParametersRemoved(
    base::Optional<cryptohome::BaseReply> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to remove firmware management parameters, error: "
               << reply->error();
  }

  progress_callbacks_.Notify(state_);
}

void AutoEnrollmentController::Timeout() {
  // When tightening the FRE flows, as a cautionary measure (to prevent
  // interference with consumer devices) timeout was chosen to only enforce FRE
  // for EXPLICTLY_REQUIRED.
  // TODO(igorcov): Investigate the remaining causes of hitting timeout and
  // potentially either remove the timeout altogether or enforce FRE in the
  // REQUIRED case as well.
  // TODO(mnissler): Add UMA to track results of auto-enrollment checks.
  if (client_start_weak_factory_.HasWeakPtrs() &&
      fre_requirement_ != FRERequirement::kExplicitlyRequired) {
    // If the callbacks to check ownership status or state keys are still
    // pending, there's a bug in the code running on the device. No use in
    // retrying anything, need to fix that bug.
    LOG(ERROR) << "Failed to start auto-enrollment check, fix the code!";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  } else {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
  }

  // Reset state.
  if (client_) {
    // Cancelling the |client_| allows it to determine whether
    // its protocol finished before login was complete.
    client_.release()->CancelAndDeleteSoon();
  }

  // Make sure to nuke pending |client_| start sequences.
  client_start_weak_factory_.InvalidateWeakPtrs();
}

}  // namespace chromeos
