// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/system_policy_request_context.h"
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
    DeviceCloudPolicyManagerChromeOS* manager,
    chromeos::DeviceSettingsService* device_settings_service)
    : local_state_(local_state),
      enterprise_service_(enterprise_service),
      consumer_service_(consumer_service),
      background_task_runner_(background_task_runner),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      device_store_(device_store),
      manager_(manager),
      device_settings_service_(device_settings_service),
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
    const std::string& auth_token,
    const AllowedDeviceModes& allowed_device_modes,
    const EnrollmentCallback& enrollment_callback) {
  DCHECK(is_initialized_);
  DCHECK(!enrollment_handler_);

  manager_->core()->Disconnect();
  enrollment_handler_.reset(new EnrollmentHandlerChromeOS(
      device_store_, install_attributes_, state_keys_broker_,
      device_settings_service_, owner_settings_service,
      CreateClient(device_management_service), background_task_runner_,
      auth_token, install_attributes_->GetDeviceId(),
      manager_->GetDeviceRequisition(), allowed_device_modes, management_mode,
      base::Bind(&DeviceCloudPolicyInitializer::EnrollmentCompleted,
                 base::Unretained(this), enrollment_callback)));
  enrollment_handler_->StartEnrollment();
}

bool DeviceCloudPolicyInitializer::ShouldAutoStartEnrollment() const {
  const RestoreMode restore_mode = GetRestoreMode();
  if (restore_mode == RESTORE_MODE_REENROLLMENT_REQUESTED ||
      restore_mode == RESTORE_MODE_REENROLLMENT_ENFORCED) {
    return true;
  }

  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentAutoStart))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  return GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey, false);
}

bool DeviceCloudPolicyInitializer::ShouldRecoverEnrollment() const {
  if (install_attributes_->IsEnterpriseDevice() &&
      chromeos::StartupUtils::IsEnrollmentRecoveryRequired()) {
    LOG(WARNING) << "Enrollment recovery required according to pref.";
    if (!DeviceCloudPolicyManagerChromeOS::GetMachineID().empty())
      return true;
    LOG(WARNING) << "Postponing recovery because machine id is missing.";
  }
  return false;
}

std::string DeviceCloudPolicyInitializer::GetEnrollmentRecoveryDomain() const {
  return install_attributes_->GetDomain();
}

bool DeviceCloudPolicyInitializer::CanExitEnrollment() const {
  if (GetRestoreMode() == RESTORE_MODE_REENROLLMENT_ENFORCED)
    return false;

  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentCanExit))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  return GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                        true);
}

std::string
DeviceCloudPolicyInitializer::GetForcedEnrollmentDomain() const {
  const base::DictionaryValue* device_state_dict =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string management_domain;
  device_state_dict->GetString(kDeviceStateManagementDomain,
                               &management_domain);
  return management_domain;
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
    StartConnection(client.Pass());
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
                            USER_AFFILIATION_NONE,
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
    manager_->StartConnection(client.Pass(), install_attributes_);
}

}  // namespace policy
