// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profile_startup.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/enterprise_extension_observer.h"
#include "chrome/browser/chromeos/gview_request_interceptor.h"
#include "chrome/browser/chromeos/network_message_observer.h"
#include "chrome/browser/chromeos/power/low_battery_observer.h"
#include "chrome/browser/chromeos/sms_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

// TODO(beng): It is very fishy that any of these services need to be tied to
//             a browser profile. This dependency should be severed and this
//             init moved earlier in startup.
void ProfileStartup(Profile* profile, bool process_startup) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the incognito profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetOffTheRecordProfile() call above.
  profile->InitChromeOSPreferences();

  if (process_startup) {
    // These observers are singletons. They are never deleted but the pointers
    // are kept in a statics so that they are not reported as leaks.
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshNotify)) {
      static chromeos::LowBatteryObserver* low_battery_observer =
          new chromeos::LowBatteryObserver(profile);
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
          low_battery_observer);
    }

    static chromeos::NetworkMessageObserver* network_message_observer =
        new chromeos::NetworkMessageObserver(profile);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddNetworkManagerObserver(network_message_observer);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddCellularDataPlanObserver(network_message_observer);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddUserActionObserver(network_message_observer);

    static chromeos::SmsObserver* sms_observer =
        new chromeos::SmsObserver(profile);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddNetworkManagerObserver(sms_observer);

    profile->SetupChromeOSEnterpriseExtensionObserver();
  }
}

}  // namespace chromeos
