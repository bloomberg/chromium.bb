// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_switches.h"

namespace chromeos {
namespace switches {

// Path for app's OEM manifest file.
const char kAppOemManifestFile[] = "app-mode-oem-manifest";

// When wallpaper boot animation is not disabled this switch
// is used to override OOBE/sign in WebUI init type.
// Possible values: parallel|postpone. Default: parallel.
const char kAshWebUIInit[] = "ash-webui-init";

// Specifies the URL of the consumer device management backend.
const char kConsumerDeviceManagementUrl[] = "consumer-device-management-url";

// Forces the stub implementation of dbus clients.
const char kDbusStub[] = "dbus-stub";

// Comma-spearated list of dbus clients that should be unstubbed.
// See chromeos/dbus/dbus_client_bundle.cc for the names of the dbus clients.
const char kDbusUnstubClients[] = "dbus-unstub-clients";

// Time before a machine at OOBE is considered derelict.
const char kDerelictDetectionTimeout[] = "derelict-detection-timeout";

// Time before a derelict machines starts demo mode.
const char kDerelictIdleTimeout[] = "derelict-idle-timeout";

// Disables wallpaper boot animation (except of OOBE case).
const char kDisableBootAnimation[] = "disable-boot-animation";

// Disables the ChromeOS demo.
const char kDisableDemoMode[] = "disable-demo-mode";

// Disable Easy sign-in.
const char kDisableEasySignin[] = "disable-easy-signin";

// Disable HID-detection OOBE screen.
const char kDisableHIDDetectionOnOOBE[] = "disable-hid-detection-on-oobe";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[] = "disable-login-animations";

// Disable login/lock UI (user pods) scrolling into view on JS side when virtual
// keyboard is shown.
const char kDisableLoginScrollIntoView[] = "disable-login-scroll-into-view";

// Disable new channel switcher UI.
const char kDisableNewChannelSwitcherUI[] = "disable-new-channel-switcher-ui";

// Disables new Kiosk UI when kiosk apps are represented as user pods.
const char kDisableNewKioskUI[] = "disable-new-kiosk-ui";

// Disable Office Editing for Docs, Sheets & Slides component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const char kDisableOfficeEditingComponentApp[] =
    "disable-office-editing-component-extension";

// Disables volume adjust sound.
const char kDisableVolumeAdjustSound[] = "disable-volume-adjust-sound";

// Disables notifications about captive portals in session.
const char kDisableNetworkPortalNotification[] =
    "disable-network-portal-notification";

// Enables switching between different cellular carriers from the UI.
const char kEnableCarrierSwitching[] = "enable-carrier-switching";

// Enables the next generation version of ChromeVox.
const char kEnableChromeVoxNext[] = "enable-chromevox-next";

// Enables consumer management, which allows user to enroll, remotely lock and
// locate the device.
const char kEnableConsumerManagement[] = "enable-consumer-management";

// If this switch is set, Chrome OS login screen uses |EmbeddedSignin| endpoint
// of GAIA.
const char kEnableEmbeddedSignin[] = "enable-embedded-signin";

// Enabled sharing assets for installed default apps.
const char kEnableExtensionAssetsSharing[]  = "enable-extension-assets-sharing";

// Enables notifications about captive portals in session.
const char kEnableNetworkPortalNotification[] =
    "enable-network-portal-notification";

// Enables rollback option on reset screen.
const char kEnableRollbackOption[] = "enable-rollback-option";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[]
    = "enable-touchpad-three-finger-click";

// Enables using screenshots in tests and seets mode.
const char kEnableScreenshotTestingWithMode[] =
    "enable-screenshot-testing-with-mode";

// Enable Kiosk mode for ChromeOS. Note this switch refers to retail mode rather
// than the kiosk app mode.
const char kEnableKioskMode[] = "enable-kiosk-mode";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[] = "enable-request-tablet-site";

// Whether to enable forced enterprise re-enrollment.
const char kEnterpriseEnableForcedReEnrollment[] =
    "enterprise-enable-forced-re-enrollment";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Don't create robot account on enrollment. Used when testing device
// enrollment against YAPS or the Python test server.
const char kEnterpriseEnrollmentSkipRobotAuth[] =
    "enterprise-enrollment-skip-robot-auth";

// Enables the chromecast support for video player app.
const char kEnableVideoPlayerChromecastSupport[] =
    "enable-video-player-chromecast-support";

// Passed to Chrome the first time that it's run after the system boots.
// Not passed on restart after sign out.
const char kFirstExecAfterBoot[] = "first-exec-after-boot";

// Usually in browser tests the usual login manager bringup is skipped so that
// tests can change how it's brought up. This flag disables that.
const char kForceLoginManagerInTests[] = "force-login-manager-in-tests";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[] = "bwsi";

// If true, the Chromebook has a Chrome OS keyboard. Don't use the flag for
// Chromeboxes.
const char kHasChromeOSKeyboard[] = "has-chromeos-keyboard";

// If true, the Chromebook has a keyboard with a diamond key.
const char kHasChromeOSDiamondKey[] = "has-chromeos-diamond-key";

// Defines user homedir. This defaults to primary user homedir.
const char kHomedir[]           = "homedir";

// With this switch, start remora OOBE with the pairing screen.
const char kHostPairingOobe[] = "host-pairing-oobe";

// If true, profile selection in UserManager will always return active user's
// profile.
// TODO(nkostlyev): http://crbug.com/364604 - Get rid of this switch after we
// turn on multi-profile feature on ChromeOS.
const char kIgnoreUserProfileMappingForTests[] =
    "ignore-user-profile-mapping-for-tests";

// Path for the screensaver used in Kiosk mode
const char kKioskModeScreensaverPath[] = "kiosk-mode-screensaver-path";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[] = "login-manager";

// Specifies the profile to use once a chromeos user is logged in.
// This parameter is ignored if user goes through login screen since user_id
// hash defines which profile directory to use.
// In case of browser restart within active session this parameter is used
// to pass user_id hash for primary user.
const char kLoginProfile[] = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[] = "login-user";

// Enables natural scroll by default.
const char kNaturalScrollDefault[] = "enable-natural-scroll-default";

// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[] = "oobe-skip-postlogin";

// Disable GAIA services such as enrollment and OAuth session restore. Used by
// 'fake' telemetry login.
const char kDisableGaiaServices[] = "disable-gaia-services";

// Interval at which we check for total time on OOBE.
const char kOobeTimerInterval[] = "oobe-timer-interval";

// Indicates that a guest session has been started before OOBE completion.
const char kOobeGuestSession[] = "oobe-guest-session";

// Specifies power stub behavior:
//  'cycle=2' - Cycles power states every 2 seconds.
// See FakeDBusThreadManager::ParsePowerCommandLineSwitch for full details.
const char kPowerStub[] = "power-stub";

// Overrides network stub behavior. By default, ethernet, wifi and vpn are
// enabled, and transitions occur instantaneously. Multiple options can be
// comma separated (no spaces). Note: all options are in the format 'foo=x'.
// See FakeShillManagerClient::SetInitialNetworkState for implementation.
// Examples:
//  'clear=1' - Clears all default configurations
//  'wifi=on' - A wifi network is initially connected ('1' also works)
//  'wifi=off' - Wifi networks are all initially disconnected ('0' also works)
//  'wifi=disabled' - Wifi is initially disabled
//  'wifi=none' - Wifi is unavailable
//  'wifi=portal' - Wifi connection will be in Portal state
//  'cellular=1' - Cellular is initially connected
//  'interactive=3' - Interactive mode, connect/scan/etc requests take 3 secs
const char kShillStub[] = "shill-stub";

// Sends test messages on first call to RequestUpdate (stub only).
const char kSmsTestMessages[] = "sms-test-messages";

// Indicates that a stub implementation of CrosSettings that stores settings in
// memory without signing should be used, treating current user as the owner.
// This option is for testing the chromeos build of chrome on the desktop only.
const char kStubCrosSettings[] = "stub-cros-settings";

// Enables animated transitions during first-run tutorial.
const char kEnableFirstRunUITransitions[] = "enable-first-run-ui-transitions";

// Forces first-run UI to be shown for every login.
const char kForceFirstRunUI[] = "force-first-run-ui";

// Enables testing for auto update UI.
const char kTestAutoUpdateUI[] = "test-auto-update-ui";

// Enables waking the device based on the receipt of some network packets.
const char kWakeOnPackets[] = "wake-on-packets";

// Screenshot testing: specifies the directory where the golden screenshots are
// stored.
const char kGoldenScreenshotsDir[] = "golden-screenshots-dir";

// Screenshot testing: specifies the directoru where artifacts will be stored.
const char kArtifactsDir[] = "artifacts-dir";

}  // namespace switches
}  // namespace chromeos
