// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace switches {

// Path for app's OEM manifest file.
const char kAppOemManifestFile[]            = "app-mode-oem-manifest";

// Enables overriding the Chrome OS board type when running on Linux.
const char kChromeOSReleaseBoard[] = "chromeos-release-board";

// Forces the stub implementation of dbus clients.
const char kDbusStub[] = "dbus-stub";

// Disable Quickoffice component app thus handlers won't be registered so
// it will be possible to install another version as normal app for testing.
const char kDisableQuickofficeComponentApp[] =
    "disable-quickoffice-component-app";

// Disables fetching online CrOS EULA page, only static version is shown.
const char kDisableOnlineEULA[] = "disable-cros-online-eula";

// Disables portal detection and network error handling before auto
// update.
const char kDisableOOBEBlockingUpdate[] =
    "disable-oobe-blocking-update";

// Disables fake ethernet network in the stub implementations.
const char kDisableStubEthernet[] = "disable-stub-ethernet";

// Enable switching between audio devices in Chrome instead of cras.
const char kEnableChromeAudioSwitching[] = "enable-chrome-audio-switching";

// Enable experimental Bluetooth features.
const char kEnableExperimentalBluetooth[] = "enable-experimental-bluetooth";

// Disables the new NetworkChangeNotifier which uses NetworkStateHandler.
const char kDisableNewNetworkChangeNotifier[] =
    "disable-new-network-change-notifier";

// Enables screensaver extensions.
const char kEnableScreensaverExtensions[] = "enable-screensaver-extensions";

// Enable "interactive" mode for stub implemenations (e.g. NetworkStateHandler)
const char kEnableStubInteractive[] = "enable-stub-interactive";

// Passed to Chrome on first boot. Not passed on restart after sign out.
const char kFirstBoot[]                     = "first-boot";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[]      = "force-login-manager-in-tests";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[]                  = "bwsi";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[]                  = "login-manager";

// Specifies a password to be used to login (along with login-user).
const char kLoginPassword[]                 = "login-password";

// Specifies the profile to use once a chromeos user is logged in.
const char kLoginProfile[]                  = "login-profile";

// Allows to override the first login screen. The value should be the name of
// the first login screen to show (see
// chrome/browser/chromeos/login/login_wizard_view.cc for actual names).
// Ignored if kLoginManager is not specified. TODO(avayvod): Remove when the
// switch is no longer needed for testing.
const char kLoginScreen[]                   = "login-screen";

// Controls the initial login screen size. Pass width,height.
const char kLoginScreenSize[]               = "login-screen-size";

// Specifies the user which is already logged in.
const char kLoginUser[]                     = "login-user";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

// Enables usage of the new ManagedNetworkConfigurationHandler and
// NetworkConfigurationHandler singletons.
const char kUseNewNetworkConfigurationHandlers[] =
    "use-new-network-configuration-handlers";

} // namespace switches
}  // namespace chromeos
