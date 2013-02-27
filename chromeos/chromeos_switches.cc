// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace switches {

// Enables overriding the Chrome OS board type when running on Linux.
const char kChromeOSReleaseBoard[] = "chromeos-release-board";

// Forces the stub implementation of dbus clients.
const char kDbusStub[] = "dbus-stub";

// Enables experiments in locally managed user creation ui.
const char kEnableLocallyManagedUserUIExperiments[] =
    "enable-locally-managed-users-ui-experiments";

// Enables the new NetworkChangeNotifier using the NetworkStateHandler class.
const char kEnableNewNetworkChangeNotifier[] =
    "enable-new-network-change-notifier";

// Enables screensaver extensions.
const char kEnableScreensaverExtensions[] = "enable-screensaver-extensions";

// Enables the new NetworkConfigurationHandler class.
const char kEnableNewNetworkConfigurationHandlers[] =
    "enable-new-network-configuration-handlers";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

}  // namespace switches
}  // namespace chromeos
