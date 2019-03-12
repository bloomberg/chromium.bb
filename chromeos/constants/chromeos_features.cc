// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {

namespace features {

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Backup.
const base::Feature kCrostiniBackup{"CrostiniBackup",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini support for usb mounting.
const base::Feature kCrostiniUsbSupport{"CrostiniUsbSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Discover Application on Chrome OS.
// If enabled, Discover App will be shown in launcher.
const base::Feature kDiscoverApp{"DiscoverApp",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables web push for background notifications in
// Android Messages Integration on Chrome OS.
const base::Feature kEnableMessagesWebPush{"EnableMessagesWebPush",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, MyFiles will be a root/volume and user can create other
// sub-folders and files in addition to the Downloads folder inside MyFiles.
const base::Feature kMyFilesVolume{"MyFilesVolume",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, will display blocking screens during re-authentication after a
// supervision transition occurred.
const base::Feature kEnableSupervisionTransitionScreens{
    "EnableSupervisionTransitionScreens", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable restriction of symlink traversal on user-supplied filesystems.
const base::Feature kFsNosymfollow{"FsNosymfollow",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable Unified Input Logic in the IME extension on Chrome OS.
const base::Feature kImeInputLogic{"ImeInputLogic",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, allows the qualified IME extension to connect to IME service.
const base::Feature kImeServiceConnectable{"ImeServiceConnectable",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable native controls in video player on Chrome OS.
const base::Feature kVideoPlayerNativeControls{
    "VideoPlayerNativeControls", base::FEATURE_ENABLED_BY_DEFAULT};

// Use the messages.google.com domain as part of the "Messages" feature under
// "Connected Devices" settings.
const base::Feature kUseMessagesGoogleComDomain{
    "UseMessagesGoogleComDomain", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables ML service inferencing (instead of TFNative inferencing)
// for the Smart Dim feature on Chrome OS.
const base::Feature kUserActivityPredictionMlService{
    "UserActivityPredictionMlService", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace chromeos
