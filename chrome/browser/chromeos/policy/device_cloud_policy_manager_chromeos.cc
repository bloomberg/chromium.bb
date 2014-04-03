// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/port.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/system_policy_request_context.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "policy/proto/device_management_backend.pb.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

namespace {

// Overridden no requisition value.
const char kNoRequisition[] = "none";

// Overridden no requisition value.
const char kRemoraRequisition[] = "remora";

// These are the machine serial number keys that we check in order until we
// find a non-empty serial number. The VPD spec says the serial number should be
// in the "serial_number" key for v2+ VPDs. However, legacy devices used a
// different key to report their serial number, which we fall back to if
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

const int
DeviceCloudPolicyManagerChromeOS::kDeviceStateKeyTimeQuantumPower;

const int
DeviceCloudPolicyManagerChromeOS::kDeviceStateKeyFutureQuanta;

DeviceCloudPolicyManagerChromeOS::DeviceCloudPolicyManagerChromeOS(
    scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    EnterpriseInstallAttributes* install_attributes)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeDevicePolicyType,
                             std::string()),
          store.get(),
          task_runner,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)),
      device_store_(store.Pass()),
      background_task_runner_(background_task_runner),
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

  InitalizeRequisition();
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
          device_store_.get(), install_attributes_, CreateClient(),
          background_task_runner_, auth_token,
          install_attributes_->GetDeviceId(), is_auto_enrollment,
          GetDeviceRequisition(), GetCurrentDeviceStateKey(),
          allowed_device_modes,
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
  if (!pref->IsDefaultValue())
    pref->GetValue()->GetAsString(&requisition);

  if (requisition == kNoRequisition)
    requisition.clear();

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
      if (requisition == kNoRequisition) {
        local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
        local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
      } else {
        local_state_->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
        local_state_->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
      }
    }
  }
}

bool DeviceCloudPolicyManagerChromeOS::ShouldAutoStartEnrollment() const {
  std::string restore_mode = GetRestoreMode();
  if (restore_mode == kDeviceStateRestoreModeReEnrollmentRequested ||
      restore_mode == kDeviceStateRestoreModeReEnrollmentEnforced) {
    return true;
  }

  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentAutoStart))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentAutoStart);

  return GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey, false);
}

bool DeviceCloudPolicyManagerChromeOS::CanExitEnrollment() const {
  if (GetRestoreMode() == kDeviceStateRestoreModeReEnrollmentEnforced)
    return false;

  if (local_state_->HasPrefPath(prefs::kDeviceEnrollmentCanExit))
    return local_state_->GetBoolean(prefs::kDeviceEnrollmentCanExit);

  return GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                        true);
}

std::string
DeviceCloudPolicyManagerChromeOS::GetForcedEnrollmentDomain() const {
  const base::DictionaryValue* device_state_dict =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string management_domain;
  device_state_dict->GetString(kDeviceStateManagementDomain,
                               &management_domain);
  return management_domain;
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
  registry->RegisterDictionaryPref(prefs::kServerBackedDeviceState);
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
  return GetMachineStatistic(chromeos::system::kHardwareClassKey);
}

// static
std::string DeviceCloudPolicyManagerChromeOS::GetCurrentDeviceStateKey() {
  std::vector<std::string> state_keys;
  if (GetDeviceStateKeys(base::Time::Now(), &state_keys) &&
      !state_keys.empty()) {
    // The key for the current time is always the first one.
    return state_keys[0];
  }

  return std::string();
}

scoped_ptr<CloudPolicyClient> DeviceCloudPolicyManagerChromeOS::CreateClient() {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      new SystemPolicyRequestContext(
          g_browser_process->system_request_context(), GetUserAgent());

  scoped_ptr<CloudPolicyClient> client(
      new CloudPolicyClient(GetMachineID(), GetMachineModel(),
                            kPolicyVerificationKeyHash,
                            USER_AFFILIATION_NONE,
                            device_status_provider_.get(),
                            device_management_service_,
                            request_context));

  // Set state keys to upload immediately after creation so the first policy
  // fetch submits them to the server.
  if (chromeos::AutoEnrollmentController::GetMode() ==
      chromeos::AutoEnrollmentController::MODE_FORCED_RE_ENROLLMENT) {
    std::vector<std::string> state_keys;
    if (GetDeviceStateKeys(base::Time::Now(), &state_keys))
      client->SetStateKeysToUpload(state_keys);
  }

  return client.Pass();
}

void DeviceCloudPolicyManagerChromeOS::EnrollmentCompleted(
    const EnrollmentCallback& callback,
    EnrollmentStatus status) {
  if (status.status() == EnrollmentStatus::STATUS_SUCCESS)
    StartConnection(enrollment_handler_->ReleaseClient());
  else
    StartIfManaged();

  enrollment_handler_.reset();
  if (!callback.is_null())
    callback.Run(status);
}

void DeviceCloudPolicyManagerChromeOS::StartIfManaged() {
  if (device_management_service_ &&
      local_state_ &&
      store()->is_initialized() &&
      store()->has_policy() &&
      !service()) {
    StartConnection(CreateClient());
  }
}

void DeviceCloudPolicyManagerChromeOS::StartConnection(
    scoped_ptr<CloudPolicyClient> client_to_connect) {
  core()->Connect(client_to_connect.Pass());
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_,
                                prefs::kDevicePolicyRefreshRate);
  attestation_policy_observer_.reset(
      new chromeos::attestation::AttestationPolicyObserver(client()));
}

void DeviceCloudPolicyManagerChromeOS::InitalizeRequisition() {
  // OEM statistics are only loaded when OOBE is not completed.
  if (chromeos::StartupUtils::IsOobeCompleted())
    return;

  const PrefService::Preference* pref = local_state_->FindPreference(
      prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    std::string requisition =
        GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey);

    if (!requisition.empty()) {
      local_state_->SetString(prefs::kDeviceEnrollmentRequisition,
                              requisition);
      if (requisition == kRemoraRequisition) {
        local_state_->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
        local_state_->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
      } else {
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentAutoStart,
            GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey,
                           false));
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentCanExit,
            GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                           false));
      }
    }
  }
}

std::string DeviceCloudPolicyManagerChromeOS::GetRestoreMode() const {
  const base::DictionaryValue* device_state_dict =
      local_state_->GetDictionary(prefs::kServerBackedDeviceState);
  std::string restore_mode;
  device_state_dict->GetString(kDeviceStateRestoreMode, &restore_mode);
  return restore_mode;
}

// static
bool DeviceCloudPolicyManagerChromeOS::GetDeviceStateKeys(
    const base::Time& timestamp,
    std::vector<std::string>* state_keys) {
  state_keys->clear();

  std::string disk_serial_number =
      GetMachineStatistic(chromeos::system::kDiskSerialNumber);
  if (disk_serial_number.empty()) {
    LOG(ERROR) << "Missing disk serial number";
    return false;
  }

  std::string machine_id = GetMachineID();
  if (machine_id.empty())
    return false;

  // Tolerate missing group code keys, some old devices may not have it.
  std::string group_code_key =
      GetMachineStatistic(chromeos::system::kOffersGroupCodeKey);

  // Get the current time in quantized form.
  int64 quantum_size = GG_INT64_C(1) << kDeviceStateKeyTimeQuantumPower;
  int64 quantized_time =
      (timestamp - base::Time::UnixEpoch()).InSeconds() & ~(quantum_size - 1);
  for (int i = 0; i < kDeviceStateKeyFutureQuanta; ++i) {
    state_keys->push_back(crypto::SHA256HashString(
        crypto::SHA256HashString(group_code_key) +
        crypto::SHA256HashString(disk_serial_number) +
        crypto::SHA256HashString(machine_id) +
        crypto::SHA256HashString(base::Int64ToString(quantized_time))));
    quantized_time += quantum_size;
  }

  return true;
}

}  // namespace policy
