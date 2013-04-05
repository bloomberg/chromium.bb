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

// Disables fake ethernet network in the stub implementations.
const char kDisableStubEthernet[] = "disable-stub-ethernet";

// Enable experimental Bluetooth features.
const char kEnableExperimentalBluetooth[] = "enable-experimental-bluetooth";

// Enables the new NetworkChangeNotifier using the NetworkStateHandler class.
const char kEnableNewNetworkChangeNotifier[] =
    "enable-new-network-change-notifier";

// Enables portal detection and network error handling before auto
// update.
const char kEnableOOBEBlockingUpdate[] =
    "enable-oobe-blocking-update";

// Enables usage of the new ManagedNetworkConfigurationHandler and
// NetworkConfigurationHandler singletons.
const char kUseNewNetworkConfigurationHandlers[] =
    "use-new-network-configuration-handlers";

// Enables screensaver extensions.
const char kEnableScreensaverExtensions[] = "enable-screensaver-extensions";

// Enable "interactive" mode for stub implemenations (e.g. NetworkStateHandler)
const char kEnableStubInteractive[] = "enable-stub-interactive";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

}  // namespace switches
}  // namespace chromeos
