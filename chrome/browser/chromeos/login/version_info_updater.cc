// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/version_info_updater.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// VersionInfoUpdater public:

VersionInfoUpdater::VersionInfoUpdater(Delegate* delegate)
    : delegate_(delegate) {
}

VersionInfoUpdater::~VersionInfoUpdater() {
}

void VersionInfoUpdater::StartUpdate(bool is_official_build) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    version_loader_.GetVersion(
        &version_consumer_,
        base::Bind(&VersionInfoUpdater::OnVersion, base::Unretained(this)),
        is_official_build ?
            VersionLoader::VERSION_SHORT_WITH_DATE :
            VersionLoader::VERSION_FULL);
    boot_times_loader_.GetBootTimes(
        &boot_times_consumer_,
        NewCallback(this, is_official_build ?
            &VersionInfoUpdater::OnBootTimesNoop :
            &VersionInfoUpdater::OnBootTimes));
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
}

void VersionInfoUpdater::UpdateVersionLabel() {
  if (!system::runtime_environment::IsRunningOnChromeOS()) {
    if (delegate_) {
      delegate_->OnOSVersionLabelTextUpdated(
          CrosLibrary::Get()->load_error_string());
    }
    return;
  }

  if (version_text_.empty())
    return;

  chrome::VersionInfo version_info;
  std::string label_text = l10n_util::GetStringFUTF8(
      IDS_LOGIN_VERSION_LABEL_FORMAT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      UTF8ToUTF16(version_info.Version()),
      UTF8ToUTF16(version_text_));

  if (!enterprise_domain_text_.empty()) {
    label_text += ' ';
    if (enterprise_status_text_.empty()) {
      label_text += l10n_util::GetStringFUTF8(
          IDS_LOGIN_MANAGED_BY_LABEL_FORMAT,
          UTF8ToUTF16(enterprise_domain_text_));
    } else {
      label_text += l10n_util::GetStringFUTF8(
          IDS_LOGIN_MANAGED_BY_WITH_STATUS_LABEL_FORMAT,
          UTF8ToUTF16(enterprise_domain_text_),
          UTF8ToUTF16(enterprise_status_text_));
    }
  }

  // Workaround over incorrect width calculation in old fonts.
  // TODO(glotov): remove the following line when new fonts are used.
  label_text += ' ';

  if (delegate_)
    delegate_->OnOSVersionLabelTextUpdated(label_text);
}

void VersionInfoUpdater::UpdateEnterpriseInfo() {
  policy::BrowserPolicyConnector* policy_connector =
      g_browser_process->browser_policy_connector();

  std::string status_text;
  policy::CloudPolicySubsystem* cloud_policy_subsystem =
      policy_connector->device_cloud_policy_subsystem();
  if (cloud_policy_subsystem) {
    switch (cloud_policy_subsystem->state()) {
      case policy::CloudPolicySubsystem::UNENROLLED:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_PENDING);
        break;
      case policy::CloudPolicySubsystem::UNMANAGED:
      case policy::CloudPolicySubsystem::BAD_GAIA_TOKEN:
      case policy::CloudPolicySubsystem::LOCAL_ERROR:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_LOST_CONNECTION);
        break;
      case policy::CloudPolicySubsystem::NETWORK_ERROR:
        status_text = l10n_util::GetStringUTF8(
            IDS_LOGIN_MANAGED_BY_STATUS_NETWORK_ERROR);
        break;
      case policy::CloudPolicySubsystem::TOKEN_FETCHED:
      case policy::CloudPolicySubsystem::SUCCESS:
        break;
    }
  }

  SetEnterpriseInfo(policy_connector->GetEnterpriseDomain(), status_text);
}

void VersionInfoUpdater::SetEnterpriseInfo(const std::string& domain_name,
                                       const std::string& status_text) {
  if (domain_name != enterprise_domain_text_ ||
      status_text != enterprise_status_text_) {
    enterprise_domain_text_ = domain_name;
    enterprise_status_text_ = status_text;
    UpdateVersionLabel();
  }
}

void VersionInfoUpdater::OnVersion(
    VersionLoader::Handle handle, std::string version) {
  version_text_.swap(version);
  UpdateVersionLabel();
}

void VersionInfoUpdater::OnBootTimesNoop(
    BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times) {
}

void VersionInfoUpdater::OnBootTimes(
    BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times) {
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

}  // namespace chromeos
