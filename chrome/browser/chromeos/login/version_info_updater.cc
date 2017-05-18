// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/version_info_updater.h"

#include <vector>

#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/version_info/version_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const char* const kReportingFlags[] = {
  chromeos::kReportDeviceVersionInfo,
  chromeos::kReportDeviceActivityTimes,
  chromeos::kReportDeviceBootMode,
  chromeos::kReportDeviceLocation,
};

// Strings used to generate the serial number part of the version string.
const char kSerialNumberPrefix[] = "SN:";

// Strings used to generate the bluetooth device name.
const char kBluetoothDeviceNamePrefix[] = "Bluetooth device name: ";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// VersionInfoUpdater public:

VersionInfoUpdater::VersionInfoUpdater(Delegate* delegate)
    : cros_settings_(chromeos::CrosSettings::Get()),
      delegate_(delegate),
      weak_pointer_factory_(this) {
}

VersionInfoUpdater::~VersionInfoUpdater() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);
}

void VersionInfoUpdater::StartUpdate(bool is_official_build) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&version_loader::GetVersion,
                   is_official_build ? version_loader::VERSION_SHORT_WITH_DATE
                                     : version_loader::VERSION_FULL),
        base::Bind(&VersionInfoUpdater::OnVersion,
                   weak_pointer_factory_.GetWeakPtr()));
  } else {
    UpdateVersionLabel();
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager) {
    policy_manager->core()->store()->AddObserver(this);

    // Ensure that we have up-to-date enterprise info in case enterprise policy
    // is already fetched and has finished initialization.
    UpdateEnterpriseInfo();
  }

  // Watch for changes to the reporting flags.
  base::Closure callback =
      base::Bind(&VersionInfoUpdater::UpdateEnterpriseInfo,
                 base::Unretained(this));
  for (unsigned int i = 0; i < arraysize(kReportingFlags); ++i) {
    subscriptions_.push_back(
        cros_settings_->AddSettingsObserver(kReportingFlags[i], callback));
  }

  // Update device bluetooth info.
  device::BluetoothAdapterFactory::GetAdapter(base::Bind(
      &VersionInfoUpdater::OnGetAdapter, weak_pointer_factory_.GetWeakPtr()));
}

void VersionInfoUpdater::UpdateVersionLabel() {
  if (version_text_.empty())
    return;

  UpdateSerialNumberInfo();

  std::string label_text = l10n_util::GetStringFUTF8(
      IDS_LOGIN_VERSION_LABEL_FORMAT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      base::UTF8ToUTF16(version_info::GetVersionNumber()),
      base::UTF8ToUTF16(version_text_), base::UTF8ToUTF16(serial_number_text_));

  // Workaround over incorrect width calculation in old fonts.
  // TODO(glotov): remove the following line when new fonts are used.
  label_text += ' ';

  if (delegate_)
    delegate_->OnOSVersionLabelTextUpdated(label_text);
}

void VersionInfoUpdater::UpdateEnterpriseInfo() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  SetEnterpriseInfo(connector->GetEnterpriseDomain(),
                    connector->GetDeviceAssetID());
}

void VersionInfoUpdater::SetEnterpriseInfo(const std::string& domain_name,
                                           const std::string& asset_id) {
  // Update the notification about device status reporting.
  if (delegate_ && !domain_name.empty()) {
    std::string enterprise_info;
    enterprise_info = l10n_util::GetStringFUTF8(
        IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, base::UTF8ToUTF16(domain_name));
    delegate_->OnEnterpriseInfoUpdated(enterprise_info, asset_id);
  }
}

void VersionInfoUpdater::UpdateSerialNumberInfo() {
  std::string serial =
      system::StatisticsProvider::GetInstance()->GetEnterpriseMachineID();
  if (!serial.empty()) {
    serial_number_text_ = kSerialNumberPrefix;
    serial_number_text_.append(serial);
  }
}

void VersionInfoUpdater::OnVersion(const std::string& version) {
  version_text_ = version;
  UpdateVersionLabel();
}

void VersionInfoUpdater::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (delegate_ && adapter->IsDiscoverable() && !adapter->GetName().empty()) {
    delegate_->OnDeviceInfoUpdated(kBluetoothDeviceNamePrefix +
                                   adapter->GetName());
  }
}

void VersionInfoUpdater::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

}  // namespace chromeos
