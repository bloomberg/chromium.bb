// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
#define CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kAutoScreenBrightness;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kBlueZLongTermKeyBlocklist;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const char kBlueZLongTermKeyBlocklistParamName[];
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kCrostiniBackup;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCrostiniUsbSupport;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kCryptAuthV2Enrollment;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDiscoverApp;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kDriveFs;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableMessagesWebPush;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kMyFilesVolume;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kEnableSupervisionTransitionScreens;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kFsNosymfollow;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS) extern const base::Feature kImeInputLogic;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kImeServiceConnectable;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kInstantTethering;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kVideoPlayerNativeControls;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesGoogleComDomain;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUseMessagesStagingUrl;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPrediction;
COMPONENT_EXPORT(CHROMEOS_CONSTANTS)
extern const base::Feature kUserActivityPredictionMlService;

}  // namespace features

}  // namespace chromeos

#endif  // CHROMEOS_CONSTANTS_CHROMEOS_FEATURES_H_
