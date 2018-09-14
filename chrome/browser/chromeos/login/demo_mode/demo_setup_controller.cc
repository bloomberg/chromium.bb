// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

constexpr char kDemoRequisition[] = "cros-demo-mode";
constexpr char kOfflineDevicePolicyFileName[] = "device_policy";
constexpr char kOfflineDeviceLocalAccountPolicyFileName[] =
    "local_account_policy";

// The policy blob data for offline demo-mode is embedded into the filesystem.
// TODO(mukai, agawronska): fix this when switching to dm-verity image.
constexpr const base::FilePath::CharType kOfflineDemoModeDir[] =
    FILE_PATH_LITERAL("/usr/share/demo_mode_resources/policy");

bool CheckOfflinePolicyFilesExist(const base::FilePath& policy_dir,
                                  std::string* message) {
  base::FilePath device_policy_path =
      policy_dir.AppendASCII(kOfflineDevicePolicyFileName);
  if (!base::PathExists(device_policy_path)) {
    *message = base::StringPrintf("Path %s does not exist",
                                  device_policy_path.AsUTF8Unsafe().c_str());
    return false;
  }
  base::FilePath local_account_policy_path =
      policy_dir.AppendASCII(kOfflineDeviceLocalAccountPolicyFileName);
  if (!base::PathExists(local_account_policy_path)) {
    *message =
        base::StringPrintf("Path %s does not exist",
                           local_account_policy_path.AsUTF8Unsafe().c_str());
    return false;
  }

  return true;
}

// Get the DeviceLocalAccountPolicyStore for the account_id.
policy::CloudPolicyStore* GetDeviceLocalAccountPolicyStore(
    const std::string& account_id) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return nullptr;

  policy::DeviceLocalAccountPolicyService* local_account_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!local_account_service)
    return nullptr;

  const std::string user_id = policy::GenerateDeviceLocalAccountUserId(
      account_id, policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  policy::DeviceLocalAccountPolicyBroker* broker =
      local_account_service->GetBrokerForUser(user_id);
  if (!broker)
    return nullptr;

  return broker->core()->store();
}

// A utility funciton of base::ReadFileToString which returns an optional
// string.
// TODO(mukai): move this to base/files.
base::Optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  std::string content;
  base::Optional<std::string> result;
  if (base::ReadFileToString(file_path, &content))
    result = std::move(content);
  return result;
}

}  //  namespace

namespace chromeos {

// static
constexpr char DemoSetupController::kDemoModeDomain[];

void DemoSetupController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDemoModeConfig,
      static_cast<int>(DemoSession::DemoModeConfig::kNone));
}

// static
bool DemoSetupController::IsDemoModeAllowed() {
  // Demo mode is only allowed on devices that support ARC++.
  return arc::IsArcAvailable();
}

// static
bool DemoSetupController::IsOfflineDemoModeAllowed() {
  // Offline demo mode can be only enabled when demo mode feature is enabled.
  return IsDemoModeAllowed() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableOfflineDemoMode);
}

// static
bool DemoSetupController::IsOobeDemoSetupFlowInProgress() {
  const WizardController* const wizard_controller =
      WizardController::default_controller();
  return wizard_controller &&
         wizard_controller->demo_setup_controller() != nullptr;
}

DemoSetupController::DemoSetupController() : weak_ptr_factory_(this) {}

DemoSetupController::~DemoSetupController() {
  if (device_local_account_policy_store_)
    device_local_account_policy_store_->RemoveObserver(this);
}

bool DemoSetupController::IsOfflineEnrollment() const {
  return demo_config_ == DemoSession::DemoModeConfig::kOffline;
}

void DemoSetupController::Enroll(OnSetupSuccess on_setup_success,
                                 OnSetupError on_setup_error) {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone)
      << "Demo config needs to be explicitly set before calling Enroll()";
  DCHECK(!enrollment_helper_);

  on_setup_success_ = std::move(on_setup_success);
  on_setup_error_ = std::move(on_setup_error);

  VLOG(1) << "Starting demo mode enrollment "
          << DemoSession::DemoConfigToString(demo_config_);

  switch (demo_config_) {
    case DemoSession::DemoModeConfig::kOnline:
      EnrollOnline();
      return;
    case DemoSession::DemoModeConfig::kOffline: {
      const base::FilePath offline_data_dir =
          policy_dir_for_tests_.empty() ? base::FilePath(kOfflineDemoModeDir)
                                        : policy_dir_for_tests_;
      EnrollOffline(offline_data_dir);
      return;
    }
    case DemoSession::DemoModeConfig::kNone:
      NOTREACHED() << "No valid demo mode config specified";
  }
}

void DemoSetupController::EnrollOnline() {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOnline);

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->GetDeviceCloudPolicyManager()->SetDeviceRequisition(
      kDemoRequisition);
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION;
  config.management_domain = DemoSetupController::kDemoModeDomain;

  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, nullptr, config, DemoSetupController::kDemoModeDomain);
  enrollment_helper_->EnrollUsingAttestation();
}

void DemoSetupController::EnrollOffline(const base::FilePath& policy_dir) {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOffline);
  DCHECK(policy_dir_.empty());
  policy_dir_ = policy_dir;

  std::string* message = new std::string();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckOfflinePolicyFilesExist, policy_dir_, message),
      base::BindOnce(&DemoSetupController::OnOfflinePolicyFilesExisted,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(message)));
}

void DemoSetupController::OnOfflinePolicyFilesExisted(std::string* message,
                                                      bool ok) {
  DCHECK_EQ(demo_config_, DemoSession::DemoModeConfig::kOffline);
  DCHECK(!policy_dir_.empty());

  if (!ok) {
    SetupFailed(*message, DemoSetupError::kRecoverable);
    return;
  }

  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_OFFLINE_DEMO;
  config.management_domain = DemoSetupController::kDemoModeDomain;
  config.offline_policy_path =
      policy_dir_.AppendASCII(kOfflineDevicePolicyFileName);
  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, nullptr /* ad_join_delegate */, config,
      DemoSetupController::kDemoModeDomain);
  enrollment_helper_->EnrollForOfflineDemo();
}

void DemoSetupController::OnAuthError(const GoogleServiceAuthError& error) {
  NOTREACHED();
}

void DemoSetupController::OnEnrollmentError(policy::EnrollmentStatus status) {
  // TODO(mukai): improve the message details.
  SetupFailed(
      base::StringPrintf(
          "EnrollmentError: status: %d client_status: %d store_status: %d "
          "validation_status: %d lock_status: %d",
          status.status(), status.client_status(), status.store_status(),
          status.validation_status(), status.lock_status()),
      DemoSetupError::kRecoverable);
}

void DemoSetupController::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  SetupFailed(base::StringPrintf("Other error: %d", error),
              DemoSetupError::kRecoverable);
}

void DemoSetupController::OnDeviceEnrolled(
    const std::string& additional_token) {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);

  // Try to load the policy for the device local account.
  if (demo_config_ == DemoSession::DemoModeConfig::kOffline) {
    DCHECK(!policy_dir_.empty());
    const base::FilePath file_path =
        policy_dir_.AppendASCII(kOfflineDeviceLocalAccountPolicyFileName);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&ReadFileToOptionalString, file_path),
        base::BindOnce(&DemoSetupController::OnDeviceLocalAccountPolicyLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  StartupUtils::MarkDeviceRegistered(
      base::BindOnce(&DemoSetupController::OnDeviceRegistered,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnMultipleLicensesAvailable(
    const EnrollmentLicenseMap& licenses) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUploadCompleted(bool success) {
  NOTREACHED();
}

void DemoSetupController::OnDeviceAttributeUpdatePermission(bool granted) {
  NOTREACHED();
}

void DemoSetupController::SetDeviceLocalAccountPolicyStoreForTest(
    policy::CloudPolicyStore* store) {
  device_local_account_policy_store_ = store;
}

void DemoSetupController::SetOfflineDataDirForTest(
    const base::FilePath& offline_dir) {
  policy_dir_for_tests_ = offline_dir;
}

void DemoSetupController::OnDeviceLocalAccountPolicyLoaded(
    base::Optional<std::string> blob) {
  if (!blob.has_value()) {
    // This is very unlikely to happen since the file existence is already
    // checked as CheckOfflinePolicyFilesExist.
    SetupFailed("Policy file for the device local account not found",
                DemoSetupError::kFatal);
    return;
  }

  enterprise_management::PolicyFetchResponse policy;
  if (!policy.ParseFromString(blob.value())) {
    SetupFailed("Error parsing local account policy blob.",
                DemoSetupError::kFatal);
    return;
  }

  // Extract the account_id from the policy data.
  enterprise_management::PolicyData policy_data;
  if (policy.policy_data().empty() ||
      !policy_data.ParseFromString(policy.policy_data())) {
    SetupFailed("Error parsing local account policy data.",
                DemoSetupError::kFatal);
    return;
  }

  // On the unittest, the device_local_account_policy_store_ is already
  // initialized. Otherwise attempts to get the store.
  if (!device_local_account_policy_store_) {
    device_local_account_policy_store_ =
        GetDeviceLocalAccountPolicyStore(policy_data.username());
  }

  if (!device_local_account_policy_store_) {
    SetupFailed("Can't find the store for the local account policy.",
                DemoSetupError::kFatal);
    return;
  }
  device_local_account_policy_store_->AddObserver(this);
  device_local_account_policy_store_->Store(policy);
}

void DemoSetupController::OnDeviceRegistered() {
  VLOG(1) << "Demo mode setup finished successfully.";
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kDemoModeConfig, static_cast<int>(demo_config_));
  prefs->CommitPendingWrite();
  Reset();
  if (!on_setup_success_.is_null())
    std::move(on_setup_success_).Run();
}

void DemoSetupController::SetupFailed(const std::string& message,
                                      DemoSetupError error) {
  Reset();
  LOG(ERROR) << message << " fatal=" << (error == DemoSetupError::kFatal);
  if (!on_setup_error_.is_null())
    std::move(on_setup_error_).Run(error);
}

void DemoSetupController::Reset() {
  DCHECK_NE(demo_config_, DemoSession::DemoModeConfig::kNone);
  DCHECK_NE(demo_config_ == DemoSession::DemoModeConfig::kOffline,
            policy_dir_.empty());
  // |demo_config_| is not reset here, because it is needed for retrying setup.
  enrollment_helper_.reset();
  policy_dir_.clear();
  if (device_local_account_policy_store_) {
    device_local_account_policy_store_->RemoveObserver(this);
    device_local_account_policy_store_ = nullptr;
  }
}

void DemoSetupController::OnStoreLoaded(policy::CloudPolicyStore* store) {
  DCHECK_EQ(store, device_local_account_policy_store_);
  StartupUtils::MarkDeviceRegistered(
      base::BindOnce(&DemoSetupController::OnDeviceRegistered,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupController::OnStoreError(policy::CloudPolicyStore* store) {
  DCHECK_EQ(store, device_local_account_policy_store_);
  SetupFailed("Failed to store the local account policy",
              DemoSetupError::kFatal);
}

}  //  namespace chromeos
