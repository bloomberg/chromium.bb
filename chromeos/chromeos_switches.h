// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CHROMEOS_SWITCHES_H_
#define CHROMEOS_CHROMEOS_SWITCHES_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace switches {

// Switches that are used in src/chromeos must go here.
// Other switches that apply just to chromeos code should go here also (along
// with any code that is specific to the chromeos system). ChromeOS specific UI
// should be in src/ash.

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
CHROMEOS_EXPORT extern const char kAppOemManifestFile[];
CHROMEOS_EXPORT extern const char kAshWebUIInit[];
CHROMEOS_EXPORT extern const char kConsumerDeviceManagementUrl[];
CHROMEOS_EXPORT extern const char kDbusStub[];
CHROMEOS_EXPORT extern const char kDerelictDetectionTimeout[];
CHROMEOS_EXPORT extern const char kDerelictIdleTimeout[];
CHROMEOS_EXPORT extern const char kDisableBootAnimation[];
CHROMEOS_EXPORT extern const char kDisableDemoMode[];
CHROMEOS_EXPORT extern const char kDisableGaiaServices[];
CHROMEOS_EXPORT extern const char kDisableGeniusApp[];
CHROMEOS_EXPORT extern const char kDisableHIDDetectionOnOOBE[];
CHROMEOS_EXPORT extern const char kDisableLoginAnimations[];
CHROMEOS_EXPORT extern const char kDisableLoginScrollIntoView[];
CHROMEOS_EXPORT extern const char kDisableNetworkPortalNotification[];
CHROMEOS_EXPORT extern const char kDisableNewChannelSwitcherUI[];
CHROMEOS_EXPORT extern const char kDisableNewKioskUI[];
CHROMEOS_EXPORT extern const char kDisableOfficeEditingComponentApp[];
CHROMEOS_EXPORT extern const char kDisableVolumeAdjustSound[];
CHROMEOS_EXPORT extern const char kEnableCarrierSwitching[];
CHROMEOS_EXPORT extern const char kEnableChromeVoxNext[];
CHROMEOS_EXPORT extern const char kEnableConsumerManagement[];
CHROMEOS_EXPORT extern const char kEnableEmbeddedSignin[];
CHROMEOS_EXPORT extern const char kEnableExtensionAssetsSharing[];
CHROMEOS_EXPORT extern const char kEnableFileManagerMTP[];
CHROMEOS_EXPORT extern const char kEnableFirstRunUITransitions[];
CHROMEOS_EXPORT extern const char kEnableKioskMode[];
CHROMEOS_EXPORT extern const char kEnableNetworkPortalNotification[];
CHROMEOS_EXPORT extern const char kEnableRequestTabletSite[];
CHROMEOS_EXPORT extern const char kEnableRollbackOption[];
CHROMEOS_EXPORT extern const char kEnableTouchpadThreeFingerClick[];
CHROMEOS_EXPORT extern const char kEnableVideoPlayerChromecastSupport[];
CHROMEOS_EXPORT extern const char kEnterpriseEnableForcedReEnrollment[];
CHROMEOS_EXPORT extern const char kEnterpriseEnrollmentInitialModulus[];
CHROMEOS_EXPORT extern const char kEnterpriseEnrollmentModulusLimit[];
CHROMEOS_EXPORT extern const char kEnterpriseEnrollmentSkipRobotAuth[];
CHROMEOS_EXPORT extern const char kFileManagerEnableNewGallery[];
CHROMEOS_EXPORT extern const char kFirstExecAfterBoot[];
CHROMEOS_EXPORT extern const char kForceFirstRunUI[];
CHROMEOS_EXPORT extern const char kForceLoginManagerInTests[];
CHROMEOS_EXPORT extern const char kGuestSession[];
CHROMEOS_EXPORT extern const char kHasChromeOSDiamondKey[];
CHROMEOS_EXPORT extern const char kHasChromeOSKeyboard[];
CHROMEOS_EXPORT extern const char kHomedir[];
CHROMEOS_EXPORT extern const char kIgnoreUserProfileMappingForTests[];
CHROMEOS_EXPORT extern const char kKioskModeScreensaverPath[];
CHROMEOS_EXPORT extern const char kLoginManager[];
CHROMEOS_EXPORT extern const char kLoginProfile[];
CHROMEOS_EXPORT extern const char kLoginUser[];
CHROMEOS_EXPORT extern const char kNaturalScrollDefault[];
CHROMEOS_EXPORT extern const char kOobeGuestSession[];
CHROMEOS_EXPORT extern const char kOobeSkipPostLogin[];
CHROMEOS_EXPORT extern const char kOobeTimerInterval[];
CHROMEOS_EXPORT extern const char kPowerStub[];
CHROMEOS_EXPORT extern const char kShillStub[];
CHROMEOS_EXPORT extern const char kShowControllerPairingDemo[];
CHROMEOS_EXPORT extern const char kShowHostPairingDemo[];
CHROMEOS_EXPORT extern const char kSmsTestMessages[];
CHROMEOS_EXPORT extern const char kStubCrosSettings[];
CHROMEOS_EXPORT extern const char kTestAutoUpdateUI[];
CHROMEOS_EXPORT extern const char kEnableScreenshotTestingWithMode[];
CHROMEOS_EXPORT extern const char kGoldenScreenshotsDir[];
CHROMEOS_EXPORT extern const char kArtifactsDir[];

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CHROMEOS_SWITCHES_H_
