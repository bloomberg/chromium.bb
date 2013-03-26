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
CHROMEOS_EXPORT extern const char kChromeOSReleaseBoard[];
CHROMEOS_EXPORT extern const char kDbusStub[];
CHROMEOS_EXPORT extern const char kDisableStubEthernet[];
CHROMEOS_EXPORT extern const char kEnableExperimentalBluetooth[];
CHROMEOS_EXPORT extern const char kEnableNewNetworkChangeNotifier[];
CHROMEOS_EXPORT extern const char kUseNewNetworkConfigurationHandlers[];
CHROMEOS_EXPORT extern const char kEnableScreensaverExtensions[];
CHROMEOS_EXPORT extern const char kEnableStubInteractive[];
CHROMEOS_EXPORT extern const char kSmsTestMessages[];

}  // namespace switches
}  // namespace chromeos

#endif  // CHROMEOS_CHROMEOS_SWITCHES_H_
