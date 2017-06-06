// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_

#include "base/feature_list.h"

namespace feature_engagement_tracker {

// A feature for enabling a demonstration mode for In-Product Help.
// This needs to be constexpr because of how it is used in
// //chrome/browser/about_flags.cc.
constexpr base::Feature kIPHDemoMode = {"IPH_DemoMode",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// All the features declared below should also be declared in the Java
// version: org.chromium.components.feature_engagement_tracker.FeatureConstants.
// These need to be constexpr, because they are used as
// flags_ui::FeatureEntry::FeatureParams in feature_list.h.
constexpr base::Feature kIPHDataSaverPreviewFeature = {
    "IPH_DataSaverPreview", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::Feature kIPHDataSaverDetailFeature = {
    "IPH_DataSaverDetail", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::Feature kIPHDownloadPageFeature = {
    "IPH_DownloadPage", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::Feature kIPHDownloadHomeFeature = {
    "IPH_DownloadHome", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_PUBLIC_FEATURE_CONSTANTS_H_
