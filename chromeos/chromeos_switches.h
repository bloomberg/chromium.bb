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
CHROMEOS_EXPORT extern const char kChromeOSReleaseBoard[];
CHROMEOS_EXPORT extern const char kDbusStub[];
CHROMEOS_EXPORT extern const char kDisableQuickofficeComponentApp[];
CHROMEOS_EXPORT extern const char kDisableOnlineEULA[];
CHROMEOS_EXPORT extern const char kDisableOOBEBlockingUpdate[];
CHROMEOS_EXPORT extern const char kDisableStubEthernet[];
CHROMEOS_EXPORT extern const char kEnableChromeAudioSwitching[];
CHROMEOS_EXPORT extern const char kEnableExperimentalBluetooth[];
CHROMEOS_EXPORT extern const char kDisableNewNetworkChangeNotifier[];
CHROMEOS_EXPORT extern const char kEnableScreensaverExtensions[];
CHROMEOS_EXPORT extern const char kEnableStubInteractive[];
CHROMEOS_EXPORT extern const char kForceLoginManagerInTests[];
CHROMEOS_EXPORT extern const char kFirstBoot[];
CHROMEOS_EXPORT extern const char kGuestSession[];
CHROMEOS_EXPORT extern const char kLoginManager[];
CHROMEOS_EXPORT extern const char kLoginPassword[];
CHROMEOS_EXPORT extern const char kLoginProfile[];
// TODO(avayvod): Remove this flag when it's unnecessary for testing
// purposes.
CHROMEOS_EXPORT extern const char kLoginScreen[];
CHROMEOS_EXPORT extern const char kLoginScreenSize[];
CHROMEOS_EXPORT extern const char kLoginUser[];
CHROMEOS_EXPORT extern const char kSmsTestMessages[];
CHROMEOS_EXPORT extern const char kUseNewNetworkConfigurationHandlers[];

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CHROMEOS_SWITCHES_H_
