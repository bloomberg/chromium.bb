// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/system_policy_request_context.h"
#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

namespace {

// Gets a machine flag from StatisticsProvider, returning the given
// |default_value| if not present.
bool GetMachineFlag(const std::string& key, bool default_value) {
  bool value = default_value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

}  // namespace

DeviceCloudPolicyInitializer::DeviceCloudPolicyInitializer(
    PrefService* local_state,
    DeviceManagementService* enterprise_service,
    DeviceManagementService* consumer_service,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    EnterpriseInstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    DeviceCloudPolicyStoreChromeOS* device_store,
    DeviceCloudPolicyManagerChromeOS* manager)
    : local_state_(local_state),
      enterprise_service_(enterprise_service),
      consumer_service_(consumer_service),
      background_task_runner_(background_task_runner),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      device_store_(device_store),
      manager_(manager),
      is_initialized_(false) {
}

DeviceCloudPolicyInitializer::~DeviceCloudPolicyInitializer() {
  DCHECK(!is_initialized_);
}

void DeviceCloudPolicyInitializer::Init() {
  DCHECK(!is_initialized_);

  is_initialized_ = true;
  device_store_->AddObserver(this);
  state_keys_update_subscription_ = state_keys_broker_->RegisterUpdateCallback(
      base::Bind(&DeviceCloudPolicyInitializer::TryToCreateClient,
                 base::Unretained(this)));

  TryToCreateClient();
}

void DeviceCloudPolicyInitializer::Shutdown() {
  DCHECK(is_initialized_);

  device_store_->RemoveObserver(this);
  enrollment_handler_.reset();
  state_keys_update_subscription_.reset();
  is_initialized_ = false;
}

void DeviceCloudPolicyInitializer::StartEnrollment(
    ManagementMode management_mode,
    DeviceManagementService* device_management_service,
    chromeos::OwnerSettingsServiceChromeOS* owner_settings_service,
    const EnrollmentConfig& enrollment_config,
    const std::string& auth_token,
    const AllowedDeviceModes& allowed_device_modes,
    const EnrollmentCallback& enrollment_callback) {
  DCHECK(is_initialized_);
  DCHECK(!enrollment_handler_);

  manager_->core()->Disconnect();
  enrollment_handler_.reset(new EnrollmentHandlerChromeOS(
      device_store_, install_attributes_, state_keys_broker_,
      owner_settings_service,
      CreateClient(device_management_service), background_task_runner_,
      enrollment_config, auth_token, install_attributes_->GetDeviceId(),
      manager_->GetDeviceRequisition(), allowed_device_modes, management_mode,
      base::Bind(&DeviceCloudPolicyInitializer::EnrollmentCompleted,
                 base::Unretained(this), enrollment_callback)));
  enrollment_handler_->StartEnrollment();
}

EnrollmentConfig DeviceCloudPolicyInitializer::GetPrescribedEnrollmentConfig()
    const {
  EnrollmentConfig config;

  const bool oobe_complete = local_state_->GetBoolean(prefs::kOobeComplete);
  if (oobe_complete && install_attributes_->IsEnterpriseDevice()) {
    // Regardless what mode is applicable, the enrollment domain is fixed.
    config.management_domain = install_attributes_->GetDomain();

    // Enrollment has completed previously and installation-time attributes
    // are in place. Enrollment recovery is required when the server
    // registration gets lost.
    if (local_state_->GetBoolean(prefs::kEnrollmentRecoveryRequired)) {
      LOG(WARNING) << "Enrollment recovery required according to pref.";
      if (DeviceCloudPolicyManagerChromeOS::GetMachineID().empty())
        LOG(WARNING) << "Postponing recovery because machine id is missing.";
      else
        config.mode = EnrollmentConfig::MODE_RECOVERY;
    }
    return config;
  }

  // OOBE is still running, or it is complete but the device hasn't been
  // enrolled yet. In either case, enrollment should take place if there's a
  // signal present that indicates the device should enroll.

  // Gather enrollment signals from various sources.
  const base::DictionaryValue* device_state =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string device_state_restore_mode;
  std::string device_state_management_domain;
  if (device_state) {
    device_state->GetString(kDeviceStateRestoreMode,
                            &device_state_restore_mode);
    device_state->GetString(kDeviceStateManagementDomain,
                            &device_state_management_domain);
  }

  const bool pref_enrollment_auto_start_present =
      local_state_->HasPrefPath(prefs::kDeviceEnrollmentAutoStart);
  const bool pref_enrollment_auto_start =
      local_state_->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  const bool pref_enrollment_can_exit_present =
      local_state_->HasPrefPath(prefs::kDeviceEnrollmentCanExit);
  const bool pref_enrollment_can_exit =
      local_state_->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  const bool oem_is_managed =
      GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey, false);
  const bool oem_can_exit_enrollment = GetMachineFlag(
      chromeos::system::kOemCanExitEnterpriseEnrollmentKey, true);

  // Decide enrollment mode. Give precedence to forced variants.
  if (device_state_restore_mode ==
      kDeviceStateRestoreModeReEnrollmentEnforced) {
    config.mode = EnrollmentConfig::MODE_SERVER_FORCED;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present &&
             pref_enrollment_auto_start &&
             pref_enrollment_can_exit_present &&
             !pref_enrollment_can_exit) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oem_is_managed && !oem_can_exit_enrollment) {
    config.mode = EnrollmentConfig::MODE_LOCAL_FORCED;
  } else if (oobe_complete) {
    // If OOBE is complete, don't return advertised modes as there's currently
    // no way to make sure advertised enrollment only gets shown once.
    config.mode = EnrollmentConfig::MODE_NONE;
  } else if (device_state_restore_mode ==
             kDeviceStateRestoreModeReEnrollmentRequested) {
    config.mode = EnrollmentConfig::MODE_SERVER_ADVERTISED;
    config.management_domain = device_state_management_domain;
  } else if (pref_enrollment_auto_start_present && pref_enrollment_auto_start) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  } else if (oem_is_managed) {
    config.mode = EnrollmentConfig::MODE_LOCAL_ADVERTISED;
  }

  return config;
}

void DeviceCloudPolicyInitializer::OnStoreLoaded(CloudPolicyStore* store) {
  TryToCreateClient();
}

void DeviceCloudPolicyInitializer::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void DeviceCloudPolicyInitializer::EnrollmentCompleted(
    const EnrollmentCallback& enrollment_callback,
    EnrollmentStatus status) {
  scoped_ptr<CloudPolicyClient> client = enrollment_handler_->ReleaseClient();
  enrollment_handler_.reset();

  if (status.status() == EnrollmentStatus::STATUS_SUCCESS) {
    StartConnection(std::move(client));
  } else {
    // Some attempts to create a client may be blocked because the enrollment
    // was in progress. We give it a try again.
    TryToCreateClient();
  }

  if (!enrollment_callback.is_null())
    enrollment_callback.Run(status);
}

scoped_ptr<CloudPolicyClient> DeviceCloudPolicyInitializer::CreateClient(
    DeviceManagementService* device_management_service) {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      new SystemPolicyRequestContext(
          g_browser_process->system_request_context(), GetUserAgent());

  return make_scoped_ptr(
      new CloudPolicyClient(DeviceCloudPolicyManagerChromeOS::GetMachineID(),
                            DeviceCloudPolicyManagerChromeOS::GetMachineModel(),
                            kPolicyVerificationKeyHash,
                            device_management_service,
                            request_context));
}

void DeviceCloudPolicyInitializer::TryToCreateClient() {
  if (!device_store_->is_initialized() ||
      !device_store_->has_policy() ||
      state_keys_broker_->pending() ||
      enrollment_handler_) {
    return;
  }

  DeviceManagementService* service = nullptr;
  if (GetManagementMode(*device_store_->policy()) ==
      MANAGEMENT_MODE_CONSUMER_MANAGED) {
    service = consumer_service_;
  } else if (GetManagementMode(*device_store_->policy()) ==
             MANAGEMENT_MODE_ENTERPRISE_MANAGED) {
    service = enterprise_service_;
  }

  if (service)
    StartConnection(CreateClient(service));
}

void DeviceCloudPolicyInitializer::StartConnection(
    scoped_ptr<CloudPolicyClient> client) {
  if (!manager_->core()->service())
    manager_->StartConnection(std::move(client), install_attributes_);
}

}  // namespace policy
