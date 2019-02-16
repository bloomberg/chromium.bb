// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

CHROMEOS_EXPORT extern const base::Feature kAndroidMessagesIntegration;
CHROMEOS_EXPORT extern const base::Feature kAutoScreenBrightness;
CHROMEOS_EXPORT extern const base::Feature kCrostiniBackup;
CHROMEOS_EXPORT extern const base::Feature kCrostiniUsbSupport;
CHROMEOS_EXPORT extern const base::Feature kCryptAuthV2Enrollment;
CHROMEOS_EXPORT extern const base::Feature kDiscoverApp;
CHROMEOS_EXPORT extern const base::Feature kDriveFs;
CHROMEOS_EXPORT extern const base::Feature kEnableMessagesWebPush;
CHROMEOS_EXPORT extern const base::Feature kMyFilesVolume;
CHROMEOS_EXPORT extern const base::Feature kEnableSupervisionTransitionScreens;
CHROMEOS_EXPORT extern const base::Feature kFsNosymfollow;
CHROMEOS_EXPORT extern const base::Feature kImeServiceConnectable;
CHROMEOS_EXPORT extern const base::Feature kInstantTethering;
CHROMEOS_EXPORT extern const base::Feature kVideoPlayerNativeControls;
CHROMEOS_EXPORT extern const base::Feature kUseMessagesGoogleComDomain;
CHROMEOS_EXPORT extern const base::Feature kUseMessagesStagingUrl;
CHROMEOS_EXPORT extern const base::Feature kUserActivityPrediction;
CHROMEOS_EXPORT extern const base::Feature kUserActivityPredictionMlService;

}  // namespace features

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
