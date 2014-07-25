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
#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "policy/proto/device_management_backend.pb.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kNoRequisition[] = "none";
const char kRemoraRequisition[] = "remora";
const char kSharkRequisition[] = "shark";

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

// Checks whether forced re-enrollment is enabled.
bool ForcedReEnrollmentEnabled() {
  return chromeos::AutoEnrollmentController::GetMode() ==
         chromeos::AutoEnrollmentController::MODE_FORCED_RE_ENROLLMENT;
}

}  // namespace

DeviceCloudPolicyManagerChromeOS::DeviceCloudPolicyManagerChromeOS(
    scoped_ptr<DeviceCloudPolicyStoreChromeOS> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ServerBackedStateKeysBroker* state_keys_broker)
    : CloudPolicyManager(
          PolicyNamespaceKey(dm_protocol::kChromeDevicePolicyType,
                             std::string()),
          store.get(),
          task_runner,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)),
      device_store_(store.Pass()),
      state_keys_broker_(state_keys_broker),
      local_state_(NULL) {
}

DeviceCloudPolicyManagerChromeOS::~DeviceCloudPolicyManagerChromeOS() {}

void DeviceCloudPolicyManagerChromeOS::Initialize(PrefService* local_state) {
  CHECK(local_state);

  local_state_ = local_state;

  state_keys_update_subscription_ = state_keys_broker_->RegisterUpdateCallback(
      base::Bind(&DeviceCloudPolicyManagerChromeOS::OnStateKeysUpdated,
                 base::Unretained(this)));

  InitializeRequisition();
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
  VLOG(1) << "SetDeviceRequisition " << requisition;
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

bool DeviceCloudPolicyManagerChromeOS::IsRemoraRequisition() const {
  return GetDeviceRequisition() == kRemoraRequisition;
}

bool DeviceCloudPolicyManagerChromeOS::IsSharkRequisition() const {
  return GetDeviceRequisition() == kSharkRequisition;
}

void DeviceCloudPolicyManagerChromeOS::Shutdown() {
  state_keys_update_subscription_.reset();
  CloudPolicyManager::Shutdown();
}

// static
void DeviceCloudPolicyManagerChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
  registry->RegisterDictionaryPref(prefs::kServerBackedDeviceState);
  registry->RegisterBooleanPref(prefs::kConsumerManagementEnrollmentRequested,
                                false);
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

void DeviceCloudPolicyManagerChromeOS::StartConnection(
    scoped_ptr<CloudPolicyClient> client_to_connect,
    scoped_ptr<CloudPolicyClient::StatusProvider> device_status_provider) {
  CHECK(!service());

  device_status_provider_ = device_status_provider.Pass();

  // Set state keys here so the first policy fetch submits them to the server.
  if (ForcedReEnrollmentEnabled())
    client_to_connect->SetStateKeysToUpload(state_keys_broker_->state_keys());

  core()->Connect(client_to_connect.Pass());
  core()->StartRefreshScheduler();
  core()->TrackRefreshDelayPref(local_state_,
                                prefs::kDevicePolicyRefreshRate);
  attestation_policy_observer_.reset(
      new chromeos::attestation::AttestationPolicyObserver(client()));
}

void DeviceCloudPolicyManagerChromeOS::OnStateKeysUpdated() {
  if (client() && ForcedReEnrollmentEnabled())
    client()->SetStateKeysToUpload(state_keys_broker_->state_keys());
}

void DeviceCloudPolicyManagerChromeOS::InitializeRequisition() {
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
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition) {
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

}  // namespace policy
