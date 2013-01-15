// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/version_info_updater.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/chromeos/chromeos_version.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

const char* kReportingFlags[] = {
  chromeos::kReportDeviceVersionInfo,
  chromeos::kReportDeviceActivityTimes,
  chromeos::kReportDeviceBootMode,
  chromeos::kReportDeviceLocation,
};

}

///////////////////////////////////////////////////////////////////////////////
// VersionInfoUpdater public:

VersionInfoUpdater::VersionInfoUpdater(Delegate* delegate)
    : cros_settings_(chromeos::CrosSettings::Get()),
      delegate_(delegate) {
}

VersionInfoUpdater::~VersionInfoUpdater() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->browser_policy_connector()->
          GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);

  for (unsigned int i = 0; i < arraysize(kReportingFlags); ++i)
    cros_settings_->RemoveSettingsObserver(kReportingFlags[i], this);
}

void VersionInfoUpdater::StartUpdate(bool is_official_build) {
  if (base::chromeos::IsRunningOnChromeOS()) {
    version_loader_.GetVersion(
        is_official_build ? VersionLoader::VERSION_SHORT_WITH_DATE
                          : VersionLoader::VERSION_FULL,
        base::Bind(&VersionInfoUpdater::OnVersion, base::Unretained(this)),
        &tracker_);
    boot_times_loader_.GetBootTimes(
        base::Bind(is_official_build ? &VersionInfoUpdater::OnBootTimesNoop
                                     : &VersionInfoUpdater::OnBootTimes,
                   base::Unretained(this)),
        &tracker_);
  } else {
    UpdateVersionLabel();
  }

  policy::CloudPolicySubsystem* cloud_policy =
      g_browser_process->browser_policy_connector()->
          device_cloud_policy_subsystem();
  if (cloud_policy) {
    // Two-step reset because we want to construct new ObserverRegistrar after
    // destruction of old ObserverRegistrar to avoid DCHECK violation because
    // of adding existing observer.
    cloud_policy_registrar_.reset();
    cloud_policy_registrar_.reset(
        new policy::CloudPolicySubsystem::ObserverRegistrar(
            cloud_policy, this));

    // Ensure that we have up-to-date enterprise info in case enterprise policy
    // is already fetched and has finished initialization.
    UpdateEnterpriseInfo();
  }

  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->browser_policy_connector()->
          GetDeviceCloudPolicyManager();
  if (policy_manager) {
    policy_manager->core()->store()->AddObserver(this);

    // Ensure that we have up-to-date enterprise info in case enterprise policy
    // is already fetched and has finished initialization.
    UpdateEnterpriseInfo();
  }

  // Watch for changes to the reporting flags.
  for (unsigned int i = 0; i < arraysize(kReportingFlags); ++i)
    cros_settings_->AddSettingsObserver(kReportingFlags[i], this);
}

void VersionInfoUpdater::UpdateVersionLabel() {
  if (version_text_.empty())
    return;

  chrome::VersionInfo version_info;
  std::string label_text = l10n_util::GetStringFUTF8(
      IDS_LOGIN_VERSION_LABEL_FORMAT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      UTF8ToUTF16(version_info.Version()),
      UTF8ToUTF16(version_text_));

  // Workaround over incorrect width calculation in old fonts.
  // TODO(glotov): remove the following line when new fonts are used.
  label_text += ' ';

  if (delegate_)
    delegate_->OnOSVersionLabelTextUpdated(label_text);
}

void VersionInfoUpdater::UpdateEnterpriseInfo() {
  SetEnterpriseInfo(
      g_browser_process->browser_policy_connector()->GetEnterpriseDomain());
}

void VersionInfoUpdater::SetEnterpriseInfo(const std::string& domain_name) {
  if (domain_name != enterprise_domain_text_) {
    enterprise_domain_text_ = domain_name;
    UpdateVersionLabel();

    // Update the notification about device status reporting.
    if (delegate_) {
      std::string enterprise_info;
      if (!domain_name.empty()) {
        enterprise_info = l10n_util::GetStringFUTF8(
            IDS_DEVICE_OWNED_BY_NOTICE,
            UTF8ToUTF16(domain_name));
        delegate_->OnEnterpriseInfoUpdated(enterprise_info);
      }
    }
  }
}

void VersionInfoUpdater::OnVersion(const std::string& version) {
  version_text_ = version;
  UpdateVersionLabel();
}

void VersionInfoUpdater::OnBootTimesNoop(
    const BootTimesLoader::BootTimes& boot_times) {}

// Mask it out for ASAN before the bug is fixed, see crbug.com/170034
#if defined(ADDRESS_SANITIZER)
__attribute__((no_address_safety_analysis))
#endif  // defined(ADDRESS_SANITIZER)
void VersionInfoUpdater::OnBootTimes(
    const BootTimesLoader::BootTimes& boot_times) {
  const char* kBootTimesNoChromeExec =
      "Non-firmware boot took %.2f seconds (kernel %.2fs, system %.2fs)";
  const char* kBootTimesChromeExec =
      "Non-firmware boot took %.2f seconds "
      "(kernel %.2fs, system %.2fs, chrome %.2fs)";
  std::string boot_times_text;

  if (boot_times.chrome > 0) {
    boot_times_text =
        base::StringPrintf(
            kBootTimesChromeExec,
            boot_times.total,
            boot_times.pre_startup,
            boot_times.system,
            boot_times.chrome);
  } else {
    boot_times_text =
        base::StringPrintf(
            kBootTimesNoChromeExec,
            boot_times.total,
            boot_times.pre_startup,
            boot_times.system);
  }
  // Use UTF8ToWide once this string is localized.
  if (delegate_)
    delegate_->OnBootTimesLabelTextUpdated(boot_times_text);
}

void VersionInfoUpdater::OnPolicyStateChanged(
    policy::CloudPolicySubsystem::PolicySubsystemState state,
    policy::CloudPolicySubsystem::ErrorDetails error_details) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED)
    UpdateEnterpriseInfo();
  else
    NOTREACHED();
}

}  // namespace chromeos
