// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace switches {

// Path for app's OEM manifest file.
const char kAppOemManifestFile[]            = "app-mode-oem-manifest";

// When wallpaper boot animation is not disabled this switch
// is used to override OOBE/sign in WebUI init type.
// Possible values: parallel|postpone. Default: parallel.
const char kAshWebUIInit[]                  = "ash-webui-init";

// Enables overriding the path for the default authentication extension.
const char kAuthExtensionPath[]             = "auth-ext-path";

// Enables overriding the Chrome OS board type when running on Linux.
const char kChromeOSReleaseBoard[] = "chromeos-release-board";

// Forces the stub implementation of dbus clients.
const char kDbusStub[] = "dbus-stub";

// All stub networks are idle by default.
const char kDefaultStubNetworkStateIdle[] = "default-stub-network-state-idle";

// Disables wallpaper boot animation (except of OOBE case).
const char kDisableBootAnimation[]          = "disable-boot-animation";

// Disables Chrome Captive Portal detector, which initiates Captive
// Portal detection for new active networks.
const char kDisableChromeCaptivePortalDetector[] =
    "disable-chrome-captive-portal-detector";

// Disables Google Drive integration.
const char kDisableDrive[]                  = "disable-drive";

// Disable policy-configured local accounts.
const char kDisableLocalAccounts[]          = "disable-local-accounts";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[]        = "disable-login-animations";

// Disable new channel switcher UI.
const char kDisableNewChannelSwitcherUI[]   = "disable-new-channel-switcher-ui";

// Disable Quickoffice component app thus handlers won't be registered so
// it will be possible to install another version as normal app for testing.
const char kDisableQuickofficeComponentApp[] =
    "disable-quickoffice-component-app";

// Disables fetching online CrOS EULA page, only static version is shown.
const char kDisableOnlineEULA[] = "disable-cros-online-eula";

// Avoid doing animations upon oobe.
const char kDisableOobeAnimation[]          = "disable-oobe-animation";

// Disables portal detection and network error handling before auto
// update.
const char kDisableOOBEBlockingUpdate[] =
    "disable-oobe-blocking-update";

// Disables fake ethernet network in the stub implementations.
const char kDisableStubEthernet[] = "disable-stub-ethernet";

// Enables overriding the path for the default echo component extension.
// Useful for testing.
const char kEchoExtensionPath[]             = "echo-ext-path";

// Enables component extension that initializes background pages of
// certain hosted applications.
const char kEnableBackgroundLoader[]        = "enable-background-loader";

// Enables switching between different cellular carriers from the UI.
const char kEnableCarrierSwitching[]        = "enable-carrier-switching";

// Enable switching between audio devices in Chrome instead of cras.
const char kEnableChromeAudioSwitching[] = "enable-chrome-audio-switching";

// Enables Chrome Captive Portal detector, which initiates Captive
// Portal detection for new active networks.
const char kEnableChromeCaptivePortalDetector[] =
    "enable-chrome-captive-portal-detector";

// Enables screensaver extensions.
const char kEnableScreensaverExtensions[] = "enable-screensaver-extensions";

// Enable "interactive" mode for stub implemenations (e.g. NetworkStateHandler)
const char kEnableStubInteractive[] = "enable-stub-interactive";

// Enable stub portalled wifi network for testing.
const char kEnableStubPortalledWifi[] = "enable-stub-portalled-wifi";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[]
    = "enable-touchpad-three-finger-click";

// Enable Kiosk mode for ChromeOS. Note this switch refers to retail mode rather
// than the kiosk app mode.
const char kEnableKioskMode[]               = "enable-kiosk-mode";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[]       = "enable-request-tablet-site";

// Enables static ip configuration. This flag should be removed when it's on by
// default.
const char kEnableStaticIPConfig[]          = "enable-static-ip-config";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Shows the selecting checkboxes in the Files.app.
const char kFileManagerShowCheckboxes[]     = "file-manager-show-checkboxes";

// Enables the webstore integration feature in the Files.app.
const char kFileManagerEnableWebstoreIntegration[] =
    "file-manager-enable-webstore-integration";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[]            = "first-exec-after-boot";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[]      = "force-login-manager-in-tests";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[]                  = "bwsi";

// If true, the Chromebook has a Chrome OS keyboard. Don't use the flag for
// Chromeboxes.
const char kHasChromeOSKeyboard[]           = "has-chromeos-keyboard";

// If true, the Chromebook has a keyboard with a diamond key.
const char kHasChromeOSDiamondKey[]         = "has-chromeos-diamond-key";

// Path for the screensaver used in Kiosk mode
const char kKioskModeScreensaverPath[]      = "kiosk-mode-screensaver-path";

// Allows override of oobe for testing - goes directly to the login screen.
const char kLoginScreen[]                   = "login-screen";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[]                  = "login-manager";

// Specifies a password to be used to login (along with login-user).
const char kLoginPassword[]                 = "login-password";

// Specifies the profile to use once a chromeos user is logged in.
const char kLoginProfile[]                  = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[]                     = "login-user";

// Enables natural scroll by default.
const char kNaturalScrollDefault[]          = "enable-natural-scroll-default";

// Disables tab discard in low memory conditions, a feature which silently
// closes inactive tabs to free memory and to attempt to avoid the kernel's
// out-of-memory process killer.
const char kNoDiscardTabs[]                 = "no-discard-tabs";

// Disables recording of swap and CPU utilization metrics logging after tab
// switch and scroll events.
const char kNoSwapMetrics[]                 = "no-swap-metrics";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[]             = "oobe-skip-postlogin";

// Skips the machine hwid check. Useful for running in VMs because they have no
// hwid.
const char kSkipHWIDCheck[]                 = "skip-hwid-check";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

// Indicates that a stub implementation of CrosSettings that stores settings in
// memory without signing should be used, treating current user as the owner.
// This option is for testing the chromeos build of chrome on the desktop only.
const char kStubCrosSettings[]              = "stub-cros-settings";

// Enables usage of the new ManagedNetworkConfigurationHandler and
// NetworkConfigurationHandler singletons.
const char kUseNewNetworkConfigurationHandlers[] =
    "use-new-network-configuration-handlers";

}  // namespace switches
}  // namespace chromeos
