// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_store.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// MachineInfo key names.
const char kMachineInfoSystemHwqual[] = "hardware_class";

// These are the machine serial number keys that we check in order until we
// find a non-empty serial number. The VPD spec says the serial number should be
// in the "serial_number" key for v2+ VPDs. However, legacy devices used a
// different keys to report their serial number, which we fall back to if
// "serial_number" is not present.
//
// Product_S/N is still special-cased due to inconsistencies with serial
// numbers on Lumpy devices: On these devices, serial_number is identical to
// Product_S/N with an appended checksum. Unfortunately, the sticker on the
// packaging doesn't include that checksum either (the sticker on the device
// does though!). The former sticker is the source of the serial number used by
// device management service, so we prefer Product_S/N over serial number to
// match the server.
//
// TODO(mnissler): Move serial_number back to the top once the server side uses
// the correct serial number.
const char* kMachineInfoSerialNumberKeys[] = {
  "Product_S/N",    // Lumpy/Alex devices
  "serial_number",  // VPD v2+ devices
  "Product_SN",     // Mario
  "sn",             // old ZGB devices (more recent ones use serial_number)
};

// Fetches a machine statistic value from StatisticsProvider, returns an empty
// string on failure.
std::string GetMachineStatistic(const std::string& key) {
  std::string value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(key, &value))
    return std::string();

  return value;
}

// Gets a machine flag from StatisticsProvider, returns the given
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

DeviceCloudPolicyManagerChromeOS::DeviceCloudPolicyManagerChromeOS(
    scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    EnterpriseInstallAttributes* install_attributes)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeDevicePolicyType,
                             std::string()),
          store.get(),
          task_runner),
      device_store_(store.Pass()),
      install_attributes_(install_attributes),
      device_management_service_(NULL),
      local_state_(NULL) {}

DeviceCloudPolicyManagerChromeOS::~DeviceCloudPolicyManagerChromeOS() {}

void DeviceCloudPolicyManagerChromeOS::Connect(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    scoped_ptr<CloudPolicyClient::StatusProvider> device_status_provider) {
  CHECK(!device_management_service_);
  CHECK(device_management_service);
  CHECK(local_state);

  local_state_ = local_state;
  device_management_service_ = device_management_service;
  device_status_provider_ = device_status_provider.Pass();

  StartIfManaged();
}

void DeviceCloudPolicyManagerChromeOS::StartEnrollment(
    const std::string& auth_token,
    bool is_auto_enrollment,
    const AllowedDeviceModes& allowed_device_modes,
    const EnrollmentCallback& callback) {
  CHECK(device_management_service_);
  core()->Disconnect();

  enrollment_handler_.reset(
      new EnrollmentHandlerChromeOS(
          device_store_.get(), install_attributes_, CreateClient(), auth_token,
          install_attributes_->GetDeviceId(), is_auto_enrollment,
          GetDeviceRequisition(), allowed_device_modes,
          base::Bind(&DeviceCloudPolicyManagerChromeOS::EnrollmentCompleted,
                     base::Unretained(this), callback)));
  enrollment_handler_->StartEnrollment();
}

void DeviceCloudPolicyManagerChromeOS::CancelEnrollment() {
  if (enrollment_handler_.get()) {
    enrollment_handler_.reset();
    StartIfManaged();
  }
}

std::string DeviceCloudPolicyManagerChromeOS::GetDeviceRequisition() const {
  std::string requisition;
  const PrefService::Preference* pref = local_state_->FindPreference(
      prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    requisition =
        GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey);
  } else {
    pref->GetValue()->GetAsString(&requisition);
  }

  return requisition;
}

void DeviceCloudPolicyManagerChromeOS::SetDeviceRequisition(
    const std::string& requisition) {
  if (local_state_) {
    if (requisition.empty()) {
      local_state_->ClearPref(prefs::kDeviceEnrollmentRequisition);
      local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
      local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
    } else {
      local_state_->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
      local_state_->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
      local_state_->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
    }
  }
}

bool DeviceCloudPolicyManagerChromeOS::ShouldAutoStartEnrollment() const {
  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentAutoStart))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  return GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey, false);
}

bool DeviceCloudPolicyManagerChromeOS::CanExitEnrollment() const {
  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentCanExit))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  return GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                        true);
}

void DeviceCloudPolicyManagerChromeOS::Shutdown() {
  CloudPolicyManager::Shutdown();
  device_status_provider_.reset();
}

void DeviceCloudPolicyManagerChromeOS::OnStoreLoaded(CloudPolicyStore* store) {
  CloudPolicyManager::OnStoreLoaded(store);

  if (!enrollment_handler_.get())
    StartIfManaged();
}

// static
void DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
}

// static
std::string DeviceCloudPolicyManagerChromeOS::GetMachineID() {
  std::string machine_id;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  for (size_t i = 0; i < arraysize(kMachineInfoSerialNumberKeys); i++) {
    if (provider->GetMachineStatistic(kMachineInfoSerialNumberKeys[i],
                                      &machine_id) &&
        !machine_id.empty()) {
      break;
    }
  }

  if (machine_id.empty())
    LOG(WARNING) << "Failed to get machine id.";

  return machine_id;
}

// static
std::string DeviceCloudPolicyManagerChromeOS::GetMachineModel() {
  return GetMachineStatistic(kMachineInfoSystemHwqual);
}

std::string DeviceCloudPolicyManagerChromeOS::GetRobotAccountId() {
  const enterprise_management::PolicyData* policy = device_store_->policy();
  return policy ? policy->service_account_identity() : std::string();
}

scoped_ptr<CloudPolicyClient> DeviceCloudPolicyManagerChromeOS::CreateClient() {
  return make_scoped_ptr(
      new CloudPolicyClient(GetMachineID(), GetMachineModel(),
                            USER_AFFILIATION_NONE,
                            device_status_provider_.get(),
                            device_management_service_));
}

void DeviceCloudPolicyManagerChromeOS::EnrollmentCompleted(
    const EnrollmentCallback& callback,
    EnrollmentStatus status) {
  if (status.status() == EnrollmentStatus::STATUS_SUCCESS) {
    core()->Connect(enrollment_handler_->ReleaseClient());
    core()->StartRefreshScheduler();
    core()->TrackRefreshDelayPref(local_state_,
                                  prefs::kDevicePolicyRefreshRate);
    attestation_policy_observer_.reset(
        new chromeos::attestation::AttestationPolicyObserver(client()));
  } else {
    StartIfManaged();
  }

  enrollment_handler_.reset();
  if (!callback.is_null())
    callback.Run(status);
}

void DeviceCloudPolicyManagerChromeOS::StartIfManaged() {
  if (device_management_service_ &&
      local_state_ &&
      store()->is_initialized() &&
      store()->is_managed() &&
      !service()) {
    core()->Connect(CreateClient());
    core()->StartRefreshScheduler();
    core()->TrackRefreshDelayPref(local_state_,
                                  prefs::kDevicePolicyRefreshRate);
    attestation_policy_observer_.reset(
        new chromeos::attestation::AttestationPolicyObserver(client()));
  }
}

}  // namespace policy
