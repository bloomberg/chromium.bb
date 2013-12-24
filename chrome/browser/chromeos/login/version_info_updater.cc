// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/version_info_updater.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/chrome_version_info.h"
#include "chromeos/settings/cros_settings_names.h"
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
      delegate_(delegate),
      weak_pointer_factory_(this) {
}

VersionInfoUpdater::~VersionInfoUpdater() {
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      g_browser_process->browser_policy_connector()->
          GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);
}

void VersionInfoUpdater::StartUpdate(bool is_official_build) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    version_loader_.GetVersion(
        is_official_build ? VersionLoader::VERSION_SHORT_WITH_DATE
                          : VersionLoader::VERSION_FULL,
        base::Bind(&VersionInfoUpdater::OnVersion,
                   weak_pointer_factory_.GetWeakPtr()),
        &tracker_);
  } else {
    UpdateVersionLabel();
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
  base::Closure callback =
      base::Bind(&VersionInfoUpdater::UpdateEnterpriseInfo,
                 base::Unretained(this));
  for (unsigned int i = 0; i < arraysize(kReportingFlags); ++i) {
    subscriptions_.push_back(
        cros_settings_->AddSettingsObserver(kReportingFlags[i],
                                            callback).release());
  }
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
  // Update the notification about device status reporting.
  if (delegate_ && !domain_name.empty()) {
    std::string enterprise_info;
    enterprise_info = l10n_util::GetStringFUTF8(
        IDS_DEVICE_OWNED_BY_NOTICE,
        UTF8ToUTF16(domain_name));
    delegate_->OnEnterpriseInfoUpdated(enterprise_info);
  }
}

void VersionInfoUpdater::OnVersion(const std::string& version) {
  version_text_ = version;
  UpdateVersionLabel();
}

void VersionInfoUpdater::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

}  // namespace chromeos
