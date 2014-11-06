// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/device_disabling_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace chromeos {
namespace system {

DeviceDisablingManager::DeviceDisablingManager(
    policy::BrowserPolicyConnectorChromeOS* browser_policy_connector)
    : browser_policy_connector_(browser_policy_connector),
      weak_factory_(this) {
}

DeviceDisablingManager::~DeviceDisablingManager() {
}

void DeviceDisablingManager::CheckWhetherDeviceDisabledDuringOOBE(
    const DeviceDisabledCheckCallback& callback) {
  if (policy::GetRestoreMode() != policy::RESTORE_MODE_DISABLED ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDeviceDisabling)) {
    // Indicate that the device is not disabled if it is not marked as such in
    // local state or device disabling has been turned off by flag.
    callback.Run(false);
    return;
  }

  if (browser_policy_connector_->GetDeviceMode() ==
          policy::DEVICE_MODE_PENDING) {
    // If the device mode is not known yet, request to be called back once it
    // becomes known.
    browser_policy_connector_->GetInstallAttributes()->ReadImmutableAttributes(
        base::Bind(
            &DeviceDisablingManager::CheckWhetherDeviceDisabledDuringOOBE,
            weak_factory_.GetWeakPtr(),
            callback));
    return;
  }

  if (browser_policy_connector_->GetDeviceMode() !=
          policy::DEVICE_MODE_NOT_SET) {
    // If the device is owned already, this method must have been called after
    // OOBE, which is an error. Indicate that the device is not disabled to
    // prevent spurious disabling. Actual device disabling after OOBE will be
    // handled elsewhere, by checking for disabled state in cros settings.
    LOG(ERROR) << "CheckWhetherDeviceDisabledDuringOOBE() called after OOBE.";
    callback.Run(false);
    return;
  }

  // The device is marked as disabled in local state (based on the device state
  // retrieved early during OOBE). Since device disabling has not been turned
  // off by flag and the device is still unowned, we honor the information in
  // local state and consider the device disabled.

  // Cache the disabled message.
  disabled_message_.clear();
  g_browser_process->local_state()->GetDictionary(
      prefs::kServerBackedDeviceState)->GetString(
          policy::kDeviceStateDisabledMessage,
          &disabled_message_);

  // Indicate that the device is disabled.
  callback.Run(true);
}

}  // namespace system
}  // namespace chromeos
