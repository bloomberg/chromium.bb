// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace policy {
class BrowserPolicyConnectorChromeOS;
}

namespace chromeos {
namespace system {

// If an enrolled device is lost or stolen, it can be remotely disabled by its
// owner. The disabling is triggered in two different ways, depending on the
// state the device is in:
// - If the device has been wiped, it will perform a hash dance during OOBE to
//   find out whether any persistent state has been stored for it on the server.
//   If so, persistent state is retrieved as a |DeviceStateRetrievalResponse|
//   protobuf, parsed and written to the |prefs::kServerBackedDeviceState| local
//   state pref. At the appropriate place in the OOBE flow, the
//   |WizardController| will call CheckWhetherDeviceDisabledDuringOOBE() to find
//   out whether the device is disabled, causing it to either show or skip the
//   device disabled screen.
// - If the device has not been wiped, the disabled state is retrieved with
//   every device policy fetch as part of the |PolicyData| protobuf, parsed and
//   written to the |chromeos::kDeviceDisabled| cros setting.
//
//   TODO(bartfab): Make this class subscribe to the cros setting and trigger
//   the device disabled screen. http://crbug.com/425574
class DeviceDisablingManager {
 public:
  using DeviceDisabledCheckCallback = base::Callback<void(bool)>;

  explicit DeviceDisablingManager(
      policy::BrowserPolicyConnectorChromeOS* browser_policy_connector);
  ~DeviceDisablingManager();

  // Returns the cached disabled message. The message is only guaranteed to be
  // up to date if the disabled screen was triggered.
  const std::string& disabled_message() const { return disabled_message_; }

  // Checks whether the device is disabled. |callback| will be invoked with the
  // result of the check.
  void CheckWhetherDeviceDisabledDuringOOBE(
      const DeviceDisabledCheckCallback& callback);

 private:
  policy::BrowserPolicyConnectorChromeOS* browser_policy_connector_;

  // A cached copy of the message to show on the device disabled screen.
  std::string disabled_message_;

  base::WeakPtrFactory<DeviceDisablingManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingManager);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_DEVICE_DISABLING_MANAGER_H_
