// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/chromeos_features.h"

namespace chromeos {

namespace features {

// Enables or disables integration with Android Messages on Chrome OS.
const base::Feature kAndroidMessagesIntegration{
    "AndroidMessagesIntegration", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables native ChromeVox support for Arc.
const base::Feature kChromeVoxArcSupport{"ChromeVoxArcSupport",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the Chrome OS Settings UI will include a menu for the unified
// MultiDevice settings.
const base::Feature kEnableUnifiedMultiDeviceSettings{
    "EnableUnifiedMultiDeviceSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the device to setup all MultiDevice services in a single workflow.
const base::Feature kEnableUnifiedMultiDeviceSetup{
    "EnableUnifiedMultiDeviceSetup", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the MultiDevice API on Chrome OS.
const base::Feature kMultiDeviceApi{"MultiDeviceApi",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace chromeos
